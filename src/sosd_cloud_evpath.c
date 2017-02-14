
#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_cloud_evpath.h"

#include "evpath.h"
#include "ev_dfg.h"

static int
simple_handler(CManager cm, void *vevent, void *client_data, attr_list attrs)
{
        buffer_rec_ptr buffer = vevent;
        if (SOSD.daemon.evpath.is_master) {
            printf("[master]: ");
        } else {
            printf("[client]: ");
        }
        printf("I got %d, %s\n", buffer->size, buffer->data);
        fflush(stdout);
        return 1;
}

/* name.........: SOSD_cloud_init
 * parameters...: argc, argv (passed in by address)
 * return val...: 0 if no errors
 * description..:
 *     This routine stands up the off-node transport services for the daemon
 *     and launches any particular threads it needs to in order to do that.
 *
 *     In the MPI-version, this function is responsible for populating the
 *     following global values.  Some reasonable values will at least need
 *     to be plugged into the SOS->config.* variables.
 *
 *        SOS->config.comm_rank
 *        SOS->config.comm_size
 *        SOS->config.comm_support = MPI_THREAD_*
 *        SOSD.daemon.cloud_sync_target_set[n]  (int: rank)
 *        SOSD.daemon.cloud_sync_target_count
 *        SOSD.daemon.cloud_sync_target
 *
 *    The SOSD.daemon.cloud_sync stuff can likely change here, if EVPATH
 *    is going to handle it's business differently.  The sync_target refers
 *    to the centralized store (here, stone?) that this daemon is pointing to
 *    for off-node transport.  The general system allows for multiple "back-
 *    plane stores" launched alongside the daemons, to provide reasonable
 *    scalability and throughput.
 */
int SOSD_cloud_init(int *argc, char ***argv) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_init.EVPATH");

    //TODO: Hardcoded node names for now. Fix when switching to ad hoc joins.
    SOSD.daemon.evpath.node_names = (char **) malloc(3 * sizeof(char *));
    SOSD.daemon.evpath.node_names[0] = (char *) malloc(3 * sizeof(char));
    SOSD.daemon.evpath.node_names[1] = (char *) malloc(3 * sizeof(char));
    SOSD.daemon.evpath.node_names[2] = NULL; 
    snprintf(SOSD.daemon.evpath.node_names[0], 3, "a");
    snprintf(SOSD.daemon.evpath.node_names[1], 3, "b");

    dlog(0, "Initializing EVPath DFG...\n");
    
    // Create the dataflow graph...
    SOSD.daemon.evpath.cm = CManager_create();
    CMlisten(SOSD.daemon.evpath.cm);
    if (SOSD.daemon.evpath.is_master) {
        SOSD.daemon.evpath.dfg_master  = EVmaster_create(SOSD.daemon.evpath.cm);
        SOSD.daemon.evpath.str_contact = EVmaster_get_contact_list(SOSD.daemon.evpath.dfg_master);
        EVmaster_register_node_list(SOSD.daemon.evpath.dfg_master, &SOSD.daemon.evpath.node_names[0]);
    }
    SOSD.daemon.evpath.source_handle =
        EVcreate_submit_handle(SOSD.daemon.evpath.cm, -1, buffer_format_list);
    SOSD.daemon.evpath.source_capabilities =
        EVclient_register_source("event source", SOSD.daemon.evpath.source_handle);
    SOSD.daemon.evpath.sink_capabilities =
        EVclient_register_sink_handler(
            SOSD.daemon.evpath.cm,
            "simple_handler",
            buffer_format_list,
            (EVSimpleHandlerFunc) simple_handler,
            NULL);

    if (SOSD.daemon.evpath.is_master) {
        // We're the MASTER / coordinator of the DFG...
        SOSD.daemon.evpath.dfg = EVdfg_create(SOSD.daemon.evpath.dfg_master);
        SOSD.daemon.evpath.src =
            EVdfg_create_source_stone(SOSD.daemon.evpath.dfg, "event source");
        EVdfg_assign_node(SOSD.daemon.evpath.src, "b");
        SOSD.daemon.evpath.sink =
            EVdfg_create_sink_stone(SOSD.daemon.evpath.dfg, "simple_handler");
        EVdfg_assign_node(SOSD.daemon.evpath.sink, "a");
        EVdfg_link_port(SOSD.daemon.evpath.src, 0, SOSD.daemon.evpath.sink);
        EVdfg_realize(SOSD.daemon.evpath.dfg);
 
        SOSD.daemon.evpath.client =
            EVclient_assoc_local(
                SOSD.daemon.evpath.cm,
                SOSD.daemon.evpath.instance_name,
                SOSD.daemon.evpath.dfg_master,
                SOSD.daemon.evpath.source_capabilities,
                SOSD.daemon.evpath.sink_capabilities);
        //TODO: Write this out to a file...
        //   fopen("/tmp/sosd_evpath_contact.keyline");

        printf("Contact list is \"%s\"\n", SOSD.daemon.evpath.str_contact);
    } else {
        //We're a CLIENT of the DFG coordinator.
        dlog(0, "Waiting for coordinator to share contact information...\n");
        while (!SOS_file_exists("/tmp/sosd_evpath_contact.keyline")) {
            usleep(100000);
        }
        //TODO: Read the contact data from the file.
        // ...
        dlog(0, "   ... Got it: %s\n");

        SOSD.daemon.evpath.client =
            EVclient_assoc(
                SOSD.daemon.evpath.cm,
                SOSD.daemon.evpath.instance_name,
                SOSD.daemon.evpath.str_contact,
                SOSD.daemon.evpath.source_capabilities,
                SOSD.daemon.evpath.sink_capabilities);
    }

    if (EVclient_ready_wait(SOSD.daemon.evpath.client) != 1) {
        dlog(0, "DFG Initialization failed! Aborting.\n");
        exit(EXIT_FAILURE);
    }

    if (SOSD.daemon.evpath.is_master == 0) {
        if (EVclient_source_active(SOSD.daemon.evpath.source_handle)) {
            //TODO: Take this out once we don't need it anymore.
            // Send a test message.
            buffer_rec rec;
            rec.data = (char *) malloc(256 * sizeof(char));
            snprintf(rec.data, 256, "Hello, world!");
            rec.size = strnlen(rec.data, 256);
            EVsubmit(source_handle, &rec, NULL);
        }
    }

    return 0;
}


/* name.......: SOSD_cloud_start
 * description: In the event that initialization and activation are not
 *    necessarily the same, when this function returns the communication
 *    between sosd instances is active, and all cloud functions are
 *    operating.
 */
int SOSD_cloud_start(void) {
    return 0;
}



/* name.......: SOSD_cloud_send
 * description: Actually send a message off-node.  (blocking)
 */
int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send.EVPATH");

    return 0;
}


/* name.......: SOSD_cloud_enqueue
 * description: Accept a message into the async send-queue.  (non-blocking)
 */
void  SOSD_cloud_enqueue(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_enqueue.EVPATH");

    return;
}


/* name.......: SOSD_cloud_fflush
 * description: Force the send-queue to flush and transmit.
 * note.......: With EVPath, this might be totally unnecessary.  (i.e. "Let EVPath handle it...")
 */
void  SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush.EVPATH");
    return;
}


/* name.......: SOSD_cloud_finalize
 * description: Shut down the cloud operation, flush / close files, etc.
 */
int   SOSD_cloud_finalize(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_finalize.EVPATH");

    return 0;
}


/* name.......: SOSD_cloud_shutdown_notice
 * description: Send notifications to any daemon ranks that are not in the
 *              business of listening to the node on the SOS_CMD_PORT socket.
 *              Only certain daemon ranks participate/call this function.
 */
void  SOSD_cloud_shutdown_notice(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_shutdown_notice.EVPATH");
    return;
}


/* name.......: SOSD_cloud_listen_loop
 * description: When there is a feedback/control mechanism in place
 *              between the daemons and a heirarchical authority / policy
 *              enactment chain, this will be the loop that is monitoring
 *              incoming messages from other sosd daemon instances.
 */
void  SOSD_cloud_listen_loop(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop");

    //if (EVclient_source_active(SOSD.daemon.evpath.source_handle)) {
    //    buffer_rec rec;
    //    rec.size = 777;
    //    EVsubmit(SOSD.daemon.evpath.source_handle, &rec, NULL);
    //}

    CMrun_network(SOSD.daemon.evpath.cm);

    return;
}


