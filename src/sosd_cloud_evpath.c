
#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_cloud_evpath.h"
#include "string.h"
#include "evpath.h"

bool SOSD_evpath_ready_to_listen;
bool SOSD_cloud_shutdown_underway;
void SOSD_evpath_register_connection(SOS_buffer *msg);

// Extract the buffer from EVPath and drop it into the SOSD
// message processing queue:
static int
SOSD_evpath_message_handler(
    CManager cm,
    void *vevent,
    void *client_data,
    attr_list attrs)
{
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_evpath_message_handler");
    buffer_rec_ptr evp_buffer = vevent;

    SOS_msg_header    header;
    SOS_buffer       *buffer; 
    SOS_buffer_init_sized_locking(SOS, &buffer, (evp_buffer->size + 1), false);
    memcpy(buffer->data, evp_buffer->data, evp_buffer->size);

    int entry        = 0;
    int entry_count  = 0;
    int displaced    = 0;
    int offset       = 0;

    SOS_buffer_unpack(buffer, &offset, "i", &entry_count);
    dlog(1, "  ... message contains %d entries.\n", entry_count);

    // Extract one-at-a-time single messages into 'msg'
    for (entry = 0; entry < entry_count; entry++) {
        dlog(1, "[ccc] ... processing entry %d of %d @ offset == %d \n",
            (entry + 1), entry_count, offset);
        memset(&header, '\0', sizeof(SOS_msg_header));
        displaced = SOS_buffer_unpack(buffer, &offset, "iigg",
                &header.msg_size,
                &header.msg_type,
                &header.msg_from,
                &header.pub_guid);
        dlog(1, "     ... header.msg_size == %d\n",
            header.msg_size);
        dlog(1, "     ... header.msg_type == %s  (%d)\n",
            SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_type);
        dlog(1, "     ... header.msg_from == %" SOS_GUID_FMT "\n",
            header.msg_from);
        dlog(1, "     ... header.pub_guid == %" SOS_GUID_FMT "\n",
            header.pub_guid);

        offset -= displaced;


        //Create a new message buffer:
        SOS_buffer *msg;
        SOS_buffer_init_sized_locking(SOS, &msg, (1 + header.msg_size), false);

        dlog(1, "[ccc] (%d of %d) <<< bringing in msg(%15s).size == %d from offset:%d\n",
                (entry + 1), entry_count, SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE),
                header.msg_size, offset);

        //Copy the data into the new message directly:
        memcpy(msg->data, (buffer->data + offset), header.msg_size);
        msg->len = header.msg_size;
        offset += header.msg_size;

        //Enqueue this new message into the local_sync:
        switch (header.msg_type) {
            case SOS_MSG_TYPE_ANNOUNCE:
            case SOS_MSG_TYPE_PUBLISH:
            case SOS_MSG_TYPE_VAL_SNAPS:
                pthread_mutex_lock(SOSD.sync.local.queue->sync_lock);
                pipe_push(SOSD.sync.local.queue->intake, &msg, 1);
                SOSD.sync.local.queue->elem_count++;
                pthread_mutex_unlock(SOSD.sync.local.queue->sync_lock);
                break;
            case SOS_MSG_TYPE_REGISTER:   SOSD_evpath_register_connection(msg);
            case SOS_MSG_TYPE_SHUTDOWN:   SOSD.daemon.running = 0;
            default:                      SOSD_handle_unknown    (msg); break;
        }
    }

    return 0;
}

// With EVPath, the aggregator has to build stones back down
// to the listeners that so that it is able to send feedback
// messages out to everyone it is in touch with.

void SOSD_evpath_register_connection(SOS_buffer *msg) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_evpath_handle_register");

    SOS_msg_header header;
    int offset = 0;

    SOS_buffer_unpack(msg, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    SOSD_evpath *evp = &SOSD.daemon.evpath;
    SOS_buffer_unpack(msg, &offset, "s",
        &evp->node[header.msg_from]->contact_string);
 

    return;
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

    SOSD_evpath_ready_to_listen = false;
    SOSD_evpath *evp = &SOSD.daemon.evpath;

    evp->meetup_path = NULL;
    evp->meetup_path = getenv("SOS_EVPATH_MEETUP");
    if ((evp->meetup_path == NULL) || (strlen(evp->meetup_path) < 1)) {
        char *meetup_error =
            "ERROR: Please set the SOS_EVPATH_MEETUP environment variable\n" \
            "       with a valid path that all instances of SOS daemons\n" \
            "       will be able to access, typically an NFS location.\n";
        fprintf(stderr, "%s", meetup_error);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    int expected_node_count =
        SOSD.daemon.aggregator_count + 
        SOSD.daemon.listener_count;

    SOS->config.comm_size = expected_node_count;;
    SOS->config.comm_support = -1; // Used for MPI only.

    // The cloud sync stuff gets calculated after we know
    // how many targets have connected as aggregators,
    // and have assigned the aggregators their internal rank
    // indices...
    SOSD.daemon.cloud_sync_target_count = SOSD.daemon.aggregator_count;

    dlog(0, "Initializing EVPath...\n");

    int aggregation_rank = -1;
    if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {
        aggregation_rank = SOSD.sos_context->config.comm_rank;
        SOSD.daemon.cloud_sync_target = -1;
    } else {
        aggregation_rank = SOSD.sos_context->config.comm_rank
            % SOSD.daemon.aggregator_count;
        SOSD.daemon.cloud_sync_target = aggregation_rank;
    }
    dlog(0, "   ... aggregation_rank: %d\n", aggregation_rank);

    char *contact_filename = (char *) calloc(2048, sizeof(char));
    snprintf(contact_filename, 2048, "%s/sosd.%05d.key",
        evp->meetup_path, aggregation_rank);
    dlog(0, "   ... contact_filename: %s\n", contact_filename);

    dlog(0, "   ... creating connection manager: ");
    evp->cm = CManager_create();
    CMlisten(evp->cm);
    SOSD_evpath_ready_to_listen = true;
    dlog(0, "done.\n");

    if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {
        dlog(0, "   ... demon role: AGGREGATOR\n");
        // Make space to track connections back to the listeners:
        dlog(0, "   ... creating objects to coordinate with listeners: ");
        evp->node = (SOSD_evpath_node **)
            malloc(expected_node_count * sizeof(SOSD_evpath_node *));
        int node_idx = 0; 
        for (node_idx = 0; node_idx < expected_node_count; node_idx++) {
            // Allocate space to store returning connections to clients...
            // NOTE: Fill in later, as clients connect.
            evp->node[node_idx] =
                (SOSD_evpath_node *) calloc(1, sizeof(SOSD_evpath_node));
            snprintf(evp->node[node_idx]->name, 256, "%d", node_idx);
        }
        dlog(0, "done.\n");

        dlog(0, "   ... configuring stones: ");
        evp->stone = EValloc_stone(evp->cm);
        EVassoc_terminal_action(
            evp->cm,
            evp->stone,
            SOSD_buffer_format_list,
            SOSD_evpath_message_handler,
            NULL);
        dlog(0, "done.\n");

        evp->string_list =
        attr_list_to_string(CMget_contact_list(evp->cm));
        dlog(0, "   ... connection string: %s\n", evp->string_list);

        FILE *contact_file;
        contact_file = fopen(contact_filename, "w");
        fprintf(contact_file, "%s", evp->string_list);
        fflush(contact_file);
        fclose(contact_file);

    } else {
        
        dlog(0, "   ... waiting for coordinator to share contact information.\n");
        while (!SOS_file_exists(contact_filename)) {
            usleep(100000);
        }

        evp->string_list = (char *)calloc(1024, sizeof(char));
        while(strnlen(evp->string_list, 1024) < 1) {
            FILE *contact_file;
            contact_file = fopen(contact_filename, "r");
            if (fgets(evp->string_list, 1024, contact_file) == NULL) {
                dlog(0, "Error reading the contact key file. Aborting.\n");
                exit(EXIT_FAILURE);
            }
            fclose(contact_file);
            usleep(500000);
        }

        dlog(0, "   ... targeting aggregator at: %s\n", evp->string_list);

        dlog(0, "   ... configuring stones: ");
        evp->stone        = EValloc_stone(evp->cm);
        evp->contact_list = attr_list_from_string(evp->string_list);
        EVassoc_bridge_action(
            evp->cm,
            evp->stone,
            evp->contact_list,
            evp->remote_stone);
        evp->source = EVcreate_submit_handle(
            evp->cm,
            evp->stone,
            SOSD_buffer_format_list);
        dlog(0, "done.\n");

        // evp->source is where we drop messages to send...
        // Example:  EVsubmit(evp->source, &msg, NULL);
        evp->string_list =
        attr_list_to_string(CMget_contact_list(evp->cm));
        dlog(0, "   ... feedback connection string: %s\n", evp->string_list);

        // Send this to the master.
        SOS_buffer *buffer;
        SOS_buffer_init_sized_locking(SOS, &buffer, 2048, false);

        SOS_msg_header header;
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = SOSD.sos_context->config.comm_rank;
        header.pub_guid = 0;

        int offset = 0;
        SOS_buffer_pack(buffer, &offset, "iigg", 
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);

        SOS_buffer_pack(buffer, &offset, "s", evp->string_list);

        header.msg_size = offset;
        offset = 0;
        
        SOS_buffer_pack(buffer, &offset, "i", header.msg_size);

        SOSD_cloud_send(buffer, NULL);
        SOS_buffer_destroy(buffer);
    }

    free(contact_filename);
    dlog(0, "   ... done.\n");

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
 * description: Send a message to the target aggregator.
 *              NOTE: For EVPath, the reply buffer is not used,
 *              since it has an async communication model.
 */
int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send.EVPATH");

    buffer_rec rec;
    rec.data = (unsigned char *) buffer->data;
    rec.size = buffer->len; 
    EVsubmit(SOSD.daemon.evpath.source, &rec, NULL);

    return 0;
}


/* name.......: SOSD_cloud_enqueue
 * description: Accept a message into the async send-queue.  (non-blocking)
 */
void  SOSD_cloud_enqueue(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_enqueue");
    SOS_msg_header header;
    int offset;

    if (SOSD_cloud_shutdown_underway) { return; }
    if (buffer->len == 0) {
        dlog(1, "ERROR: You attempted to enqueue a zero-length message.\n");
        return;
    }

    memset(&header, '\0', sizeof(SOS_msg_header));

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);

    dlog(6, "Enqueueing a %s message of %d bytes...\n", SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_size);
    if (buffer->len != header.msg_size) { dlog(1, "  ... ERROR: buffer->len(%d) != header.msg_size(%d)", buffer->len, header.msg_size); }

    pthread_mutex_lock(SOSD.sync.cloud_send.queue->sync_lock);
    pipe_push(SOSD.sync.cloud_send.queue->intake, (void *) &buffer, sizeof(SOS_buffer *));
    SOSD.sync.cloud_send.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.cloud_send.queue->sync_lock);

    dlog(1, "  ... done.\n");
   return;
}


/* name.......: SOSD_cloud_fflush
 * description: Force the send-queue to flush and transmit.
 * note.......: With EVPath, this might be totally unnecessary.  (i.e. "Let EVPath handle it...")
 */
void  SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush.EVPATH");

    // NOTE: This is unused with EVPath.

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

    while(!SOSD_evpath_ready_to_listen) {
            usleep(50000);
    }

    CMrun_network(SOSD.daemon.evpath.cm);

    return;
}


