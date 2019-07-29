
#include <string.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sos_target.h"
#include "sosd.h"
#include "sosd_cloud_evpath.h"

#include "evpath.h"
#include <errno.h>
#include <time.h> // nanosleep
#include <stdlib.h> // srand, rand

bool SOSD_evpath_ready_to_listen = false;
bool SOSD_cloud_shutdown_underway = false;


// Only need ONE of each of these things.
static CManager _cm = NULL;

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
                &header.ref_guid);
        dlog(1, "     ... header.msg_size == %d\n",
            header.msg_size);
        dlog(1, "     ... header.msg_type == %s  (%d)\n",
            SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_type);
        dlog(1, "     ... header.msg_from == %" SOS_GUID_FMT "\n",
            header.msg_from);
        dlog(1, "     ... header.ref_guid == %" SOS_GUID_FMT "\n",
            header.ref_guid);

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

            case SOS_MSG_TYPE_QUERY:
                SOSD_evpath_handle_parallel_query(msg);


                //...

                break;

            case SOS_MSG_TYPE_REGISTER:
                SOSD_aggregator_register_listener(msg);
                break;

            case SOS_MSG_TYPE_SHUTDOWN:
                SOSD.daemon.running = 0;
                SOSD.sos_context->status = SOS_STATUS_SHUTDOWN;
                SOS_buffer *shutdown_msg;
                SOS_buffer *shutdown_rep;
                SOS_buffer_init_sized_locking(SOS, &shutdown_msg, 1024, false);
                SOS_buffer_init_sized_locking(SOS, &shutdown_rep, 1024, false);
                offset = 0;
                SOS_buffer_pack(shutdown_msg, &offset, "i", offset);
                SOSD_send_to_self(shutdown_msg, shutdown_rep);
                SOS_buffer_destroy(shutdown_msg);
                SOS_buffer_destroy(shutdown_rep);
                break;

            case SOS_MSG_TYPE_TRIGGERPULL:
                SOSD_cloud_handle_triggerpull(msg);
                break;

            case SOS_MSG_TYPE_ACK:
                dlog(5, "sosd(%d) received ACK message"
                    " from rank %" SOS_GUID_FMT " !\n",
                        SOSD.sos_context->config.comm_rank, header.msg_from);
                break;

            default:    SOSD_handle_unknown    (msg); break;
        }
    }

    return 0;
}


void
SOSD_evpath_handle_parallel_query(SOS_buffer *msg)
{
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_evpath_handle_parallel_query");
    //This can be one of THREE things:
    //  1. We're receiving a request to process our part of a query.
    //  2. We're responsible for forwarding this message down to our
    //     attached listeners. We don't do any further work in this case.
    //  3. This is a reply we need to drop into the compiled
    //     results being assembled.
    //...The answer here is in the topology flag baked into the query message.

    SOS_msg_header  header;
    SOS_topology    topology;
    int             offset = 0;

    offset = 0;
    SOS_msg_unzip(msg, &header, 0, &offset);

    SOS_buffer_unpack(msg, &offset, "i", &topology);

    switch (topology) {
        case SOS_TOPOLOGY_DEFAULT:
            break;

        case SOS_TOPOLOGY_ALL_AGGREGATORS:
            break;

        case SOS_TOPOLOGY_ALL_LISTENERS:
            break;

        case SOS_TOPOLOGY_ATTACHED_LISTENERS:
            break;

        //TODO: QUERY (this might get removed)
        case SOS_TOPOLOGY_REPLY_PRE_MERGE:
        case SOS_TOPOLOGY_REPLY_POST_MERGE:
        default:
            //TODO: This should not happen. Error
            break;
    }

    return;
}


void SOSD_aggregator_register_listener(SOS_buffer *msg) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_aggregator_register_listener.EVPATH");

    dlog(3, "Registering a new connection...");

    SOS_msg_header header;
    int offset = 0;
    SOS_buffer_unpack(msg, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.ref_guid);

    SOSD_evpath *evp = &SOSD.daemon.evpath;

    if (header.msg_from >= SOS->config.comm_size) {
        fprintf(stderr, "ERROR: You are attempting to register a rank"
                "(%" SOS_GUID_FMT ") outside the size (%d) that you"
                " specified to the daemon at launch.",
                header.msg_from,
                SOS->config.comm_size);
        fflush(stderr);
    }

    SOSD_evpath_node *node = evp->node[header.msg_from];

    node->contact_string = NULL;
    SOS_buffer_unpack_safestr(msg, &offset, &node->contact_string);

    dlog(3, "   ... sosd(%" SOS_GUID_FMT ") gave us contact string: %s\n",
        header.msg_from,
        node->contact_string);


    dlog(3, "   ... constructing stone path: \n");

    node->out_stone    = EValloc_stone(_cm);
    node->contact_list = attr_list_from_string(node->contact_string);

    // UDP requires a longer time out with EVPath
    static atom_t CM_ENET_CONN_TIMEOUT = -1;
    CM_ENET_CONN_TIMEOUT = attr_atom_from_string("CM_ENET_CONN_TIMEOUT");
    set_int_attr(node->contact_list, CM_ENET_CONN_TIMEOUT, 60000); /* 60 seconds */
    EVaction rc;
    srand(getpid());
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 100000000ULL * rand() / RAND_MAX;
    do {
        rc = EVassoc_bridge_action(
            _cm,
            node->out_stone,
            node->contact_list,
            node->rmt_stone);
        if (rc == -1) {
            printf("SOSD listener %d: failed to connect, trying again in %lu nanoseconds...\n", SOSD.sos_context->config.comm_rank, delay.tv_nsec);
            nanosleep(&delay, NULL);
        }
    } while (rc == -1);
    node->src = EVcreate_submit_handle(
        _cm,
        node->out_stone,
        SOSD_buffer_format_list);

    node->active = true;
    dlog(3, "done.\n");

    // Send them back an ACK.

    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &reply, 128, false);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ACK;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;

    int msg_count = 1;
    offset = 0;
    SOS_buffer_pack(reply, &offset, "iiigg",
        msg_count,
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.ref_guid);

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(reply, &offset, "ii",
        msg_count,
        header.msg_size);

    buffer_rec rec;
    rec.data = (unsigned char *) reply->data;
    rec.size = reply->len;
    EVsubmit(node->src, &rec, NULL);

    dlog(3, "Registration complete.\n");

    return;
}

void
SOSD_cloud_send_to_topology(SOS_buffer *msg, SOS_topology topology)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_cloud_send_to_topology");
    if ((topology == SOS_TOPOLOGY_REPLY_PRE_MERGE)
     || (topology == SOS_TOPOLOGY_REPLY_POST_MERGE)) {
            dlog(0, "ERROR: Function called with REPLY_PRE/POST_MERGE flag."
                    " This function supports outbound messages only. Doing"
                    " nothing.\n");
            return;
    }
    
    //TODO: QUERY
    //Here we're going to dispatch the message to the appropriate [sub]set.
    //It gets wrapped like a typical direct P2P cloud missive.
    //The handler on the other end deals with it being a shard of a larger
    //query, and sends any results back here to get processed after the fact.

     

    switch (topology) {
        case SOS_TOPOLOGY_ALL_AGGREGATORS:
            break;
        case SOS_TOPOLOGY_ALL_LISTENERS:
            break;
        case SOS_TOPOLOGY_ATTACHED_LISTENERS:
            break;
    }

    return;
}


// NOTE: Trigger pulls do not flow out beyond the node where
//       they are pulled (at this time).  They go "downstream"
//       from AGGREGATOR->LISTENER and LISTENER->LOCALAPPS
void SOSD_cloud_handle_triggerpull(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_cloud_handle_triggerpull.EVPATH");

    dlog(4, "Message received... unzipping.\n");

    SOS_msg_header header;
    int offset = 0;
    SOS_msg_unzip(msg, &header, 0, &offset);

    int offset_after_original_header = offset;

    dlog(4, "Done unzipping.  offset_after_original_header == %d\n",
            offset_after_original_header);

    if ((SOS->role == SOS_ROLE_AGGREGATOR)
     && (SOS->config.comm_size > 1)) {
        // TODO { EVPATH }: This is not a robust way to determine
        //                  if *this* aggregator has listeners, we might not.
        
        dlog(4, "I am an aggregator, and I have some"
                " listener[s] to notify.\n");

        SOSD_evpath *evp = &SOSD.daemon.evpath;
        buffer_rec rec;

        dlog(2, "Wrapping the trigger message...\n");

        SOS_buffer *wrapped_msg;
        SOS_buffer_init_sized_locking(SOS, &wrapped_msg, (msg->len + 4 + 1), false);

        int msg_count = 1;
        header.msg_size = msg->len;
        header.msg_type = SOS_MSG_TYPE_TRIGGERPULL;
        header.msg_from = SOS->config.comm_rank;
        header.ref_guid = 0;

        offset = 0;
        SOS_buffer_pack(wrapped_msg, &offset, "i", msg_count);
        int offset_after_wrapped_header = offset;
        offset = 0;

        SOS_buffer_grow(wrapped_msg, msg->len + 1, SOS_WHOAMI);
        memcpy(wrapped_msg->data + offset_after_wrapped_header,
                msg->data,
                msg->len);
        wrapped_msg->len = (msg->len + offset_after_wrapped_header);
        offset = wrapped_msg->len;

        header.msg_size = offset;
        offset = 0;
        dlog(4, "Tacking on the newly wrapped message size...\n");
        dlog(4, "   header.msg_size == %d\n", header.msg_size);
        SOS_buffer_pack(wrapped_msg, &offset, "ii",
            msg_count,
            header.msg_size);


        
        // TODO: { FEEDBACK, MEMORY LEAK } Does this leak the wrapped_msg
        // struct?  We might need to use raw-allocated memory for the data
        // since EVPath is going to free it internally after transmission.
        int id = 0;
        for (id = 0; id < SOS->config.comm_size; id++) {
            if (evp->node[id]->active == true) {
                dlog(2, "   ...sending feedback msg to sosd(%d).\n", id);
                rec.data = (unsigned char *) wrapped_msg->data;
                rec.size = wrapped_msg->len;
                EVsubmit(evp->node[id]->src, &rec, NULL);
            }
        }
    }

    // Both Aggregators and Listeners should drop the feedback into
    // their queues in case they have local processes that have
    // registered sensitivity...

    offset = offset_after_original_header;

    char *handle = NULL;
    char *message = NULL;
    int message_len = -1;

    SOS_buffer_unpack_safestr(msg, &offset, &handle);
    SOS_buffer_unpack(msg, &offset, "i", &message_len);
    SOS_buffer_unpack_safestr(msg, &offset, &message);

    // SLICE
    //fprintf(stderr, "sosd(%d) got a TRIGGERPULL message from"
    //        " sosd(%" SOS_GUID_FMT ") of %d bytes in length.\n",
    //        SOS->config.comm_rank,
    //        header.msg_from,
    //        header.msg_size);
    //fflush(stderr);

    SOSD_feedback_task *task;
    task = calloc(1, sizeof(SOSD_feedback_task));
    task->type = SOS_FEEDBACK_TYPE_PAYLOAD;
    SOSD_feedback_payload *payload = calloc(1, sizeof(SOSD_feedback_payload));

    payload->handle = handle;
    payload->size = message_len;
    payload->data = (void *) message;

    task->ref = (void *) payload;
    pthread_mutex_lock(SOSD.sync.feedback.queue->sync_lock);
    pipe_push(SOSD.sync.feedback.queue->intake, (void *) &task, 1);
    SOSD.sync.feedback.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.feedback.queue->sync_lock);

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
 *    to the centralized store (here, stone) that this daemon is pointing to
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
        evp->meetup_path = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
        if (!getcwd(evp->meetup_path, SOS_DEFAULT_STRING_LEN)) {
            fprintf(stderr, "ERROR: The SOS_EVPATH_MEETUP evar was not set,"
                    " and getcwd() failed! Set the evar and retry.\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
        if (SOSD.sos_context->config.comm_rank == 0) {
            fprintf(stderr, "\n\nSTATUS: The SOS_EVPATH_MEETUP evar was not set.\n"
                    "STATUS: Using getcwd() path: %s\n", evp->meetup_path);
            fflush(stderr);
        }
    }

    int expected_node_count =
        SOSD.daemon.aggregator_count +
        SOSD.daemon.listener_count;

    SOS->config.comm_size = expected_node_count;;
    SOS->config.comm_support = -1; // Used for MPI only.

    // Do some sanity checks.
    if (SOSD.daemon.aggregator_count == 0) {
        fprintf(stderr, "ERROR: SOS requires an aggregator.\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    if ((SOS->config.comm_rank < 0)
        || (SOS->config.comm_rank >= expected_node_count)) {
        fprintf(stderr, "ERROR: SOS rank %d is outside the bounds of"
                " ranks expected (%d).\n",
                SOS->config.comm_rank,
                expected_node_count);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    if ((SOS->role == SOS_ROLE_LISTENER)
        && (SOS->config.comm_rank < SOSD.daemon.aggregator_count)) {
        fprintf(stderr, "ERROR: SOS listener(%d) was assigned a rank"
                " inside the range reserved for aggregators (0-%d).\n",
                SOS->config.comm_rank,
                (SOSD.daemon.aggregator_count - 1));
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    if ((SOS->role == SOS_ROLE_AGGREGATOR)
        && (SOS->config.comm_rank >= SOSD.daemon.aggregator_count)) {
        fprintf(stderr, "ERROR: SOS aggregator(%d) was assigned a rank"
                " outside the range reserved for aggregators (0-%d).\n",
                SOS->config.comm_rank,
                (SOSD.daemon.aggregator_count - 1));
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    // The cloud sync stuff gets calculated after we know
    // how many targets have connected as aggregators,
    // and have assigned the aggregators their internal rank
    // indices...
    SOSD.daemon.cloud_sync_target_count = SOSD.daemon.aggregator_count;

    dlog(1, "Initializing EVPath...\n");

    int aggregation_rank = -1;
    if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {
        aggregation_rank = SOSD.sos_context->config.comm_rank;
        SOSD.daemon.cloud_sync_target = -1;
    } else {
        aggregation_rank = SOSD.sos_context->config.comm_rank
            % SOSD.daemon.aggregator_count;
        SOSD.daemon.cloud_sync_target = aggregation_rank;
    }
    dlog(1, "   ... aggregation_rank: %d\n", aggregation_rank);

    char *contact_filename = (char *) calloc(2048, sizeof(char));
    snprintf(contact_filename, 2048, "%s/sosd.%05d.key",
        evp->meetup_path, aggregation_rank);
    char *present_filename = (char *) calloc(2048, sizeof(char));
    snprintf(present_filename, 2048, "%s/sosd.%05d.id",
        evp->meetup_path, SOSD.sos_context->config.comm_rank);
    dlog(1, "   ... contact_filename: %s\n", contact_filename);

    dlog(1, "   ... creating connection manager:\n");
    dlog(1, "      ... evp->recv.cm\n");

    if (_cm == NULL) {
        _cm = CManager_create();
        //...
        if (SOSD.sos_context->config.options->udp_enabled == true) {
            // UDP-style EVPath behavior.  (Potentially lossy?)
            atom_t CM_TRANSPORT = attr_atom_from_string("CM_TRANSPORT");
            attr_list listen_list = create_attr_list();
            add_attr(listen_list, CM_TRANSPORT, Attr_String, (attr_value) strdup("enet"));
            CMlisten_specific(_cm, listen_list);
            // Sanity check for this style of connection.
            char *actual_transport = NULL;
            get_string_attr(CMget_contact_list(_cm), CM_TRANSPORT, &actual_transport);
            if (!actual_transport || 
                    (strncmp(actual_transport, "enet", strlen(actual_transport)) != 0)) {
                printf("Failed to load transport \"%s\"\n", "enet");
                printf("Got transport \"%s\"\n", actual_transport);
            }
        } else {
            // ...
            // Traditional EVPath communication (TCP/IP)
            CMlisten(_cm);
        }

        CMfork_comm_thread(_cm);
    }
    
    SOSD_evpath_ready_to_listen = true;
    dlog(1, "      ... configuring stones:\n");
    evp->recv.out_stone = EValloc_stone(_cm);
    EVassoc_terminal_action(
            _cm,
            evp->recv.out_stone,
            SOSD_buffer_format_list,
            SOSD_evpath_message_handler,
            NULL);
    dlog(1, "      ... evp->send.cm\n");
    evp->send.out_stone = EValloc_stone(_cm);
    dlog(1, "      ... done.\n");
    dlog(1, "  ... done.\n");

    // Get the location we're listening on...
    evp->recv.contact_string =
        attr_list_to_string(CMget_contact_list(_cm));

    if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {

        // AGGREGATOR
        //   ... the aggregator needs to wait on the registration messages
        //   before being able to create sending stones.

        dlog(1, "   ... demon role: AGGREGATOR\n");
        // Make space to track connections back to the listeners:
        dlog(1, "   ... creating objects to coordinate with listeners: ");
        evp->node = (SOSD_evpath_node **)
            malloc(expected_node_count * sizeof(SOSD_evpath_node *));
        int node_idx = 0;
        for (node_idx = 0; node_idx < expected_node_count; node_idx++) {
            // Allocate space to store connections to LISTENERS...
            // NOTE: Filled in later, as LISTENERS connect during their init..
            // NOTE: We also allocate space to store AGGREGATOR peer connections.
            evp->node[node_idx] =
                (SOSD_evpath_node *) calloc(1, sizeof(SOSD_evpath_node));
            snprintf(evp->node[node_idx]->name, 256, "%d", node_idx);
            evp->node[node_idx]->active            = false;
            evp->node[node_idx]->contact_string    = NULL;
            evp->node[node_idx]->src               = NULL;
            evp->node[node_idx]->out_stone         = 0;
            evp->node[node_idx]->rmt_stone         = 0;
        }
        dlog(1, "done.\n");

        FILE *contact_file;
        // set the node id before we use it.
        SOSD.sos_context->config.node_id = (char *) malloc( SOS_DEFAULT_STRING_LEN );
        gethostname( SOSD.sos_context->config.node_id, SOS_DEFAULT_STRING_LEN );
        contact_file = fopen(contact_filename, "w");
        fprintf(contact_file, "%s\n%s\n%s\n",
                evp->recv.contact_string,
                SOSD.sos_context->config.node_id,
                SOSD.net->local_port);
        fflush(contact_file);
        fclose(contact_file);

        //TODO: QUERY
        //NOTE: Here we want to set up a loop to move through the .key files,
        //      much as the listener does below, but for all of the ranks of
        //      aggregators, so we can build connections to them for use in
        //      dispatching parallel query messages.
        
        // ...

    } else {

        //LISTENER

        dlog(1, "   ... waiting for coordinator to share contact"
                " information.\n");
        while (!SOS_file_exists(contact_filename)) {
            usleep(100000);
        }

        evp->send.contact_string = (char *)calloc(1025, sizeof(char));
        while(strnlen(evp->send.contact_string, 1024) < 1) {
            FILE *contact_file;
            contact_file = fopen(contact_filename, "r");
            int rc = fscanf(contact_file, "%1024s\n",
                    evp->send.contact_string);
            if (rc == EOF || strlen(evp->send.contact_string) < 1) {
                dlog(1, "Error reading the contact key file. Aborting.\n%s\n", 
                        strerror(errno));
                exit(EXIT_FAILURE);
            }
            fclose(contact_file);
            usleep(500000);
        }

        dlog(1, "   ... targeting aggregator at: %s\n", evp->send.contact_string);
        dlog(1, "   ... configuring stones:\n");
        evp->send.contact_list = attr_list_from_string(evp->send.contact_string);
        dlog(1, "      ... try: bridge action.\n");
        EVaction rc;
        srand(getpid());
        struct timespec delay;
        static atom_t CM_ENET_CONN_TIMEOUT = -1;
        CM_ENET_CONN_TIMEOUT = attr_atom_from_string("CM_ENET_CONN_TIMEOUT");
        set_int_attr(evp->send.contact_list, CM_ENET_CONN_TIMEOUT, 60000); /* 60 seconds */
        delay.tv_sec = 0;
        delay.tv_nsec = 100000000ULL * rand() / RAND_MAX;
        //printf("SOSD listener %d: sleeping for %lu nanoseconds...\n", SOSD.sos_context->config.comm_rank, delay.tv_nsec);
        do {
            nanosleep(&delay, NULL);
            rc = EVassoc_bridge_action(
                    _cm,
                    evp->send.out_stone,
                    evp->send.contact_list,
                    evp->send.rmt_stone);
            if (rc == -1) {
                printf("SOSD listener %d: failed to connect, trying again in %lu nanoseconds...\n", SOSD.sos_context->config.comm_rank, delay.tv_nsec);
            }
        } while (rc == -1);
        dlog(1, "      ... try: submit handle.\n");
        evp->send.src = EVcreate_submit_handle(
                _cm,
                evp->send.out_stone,
                SOSD_buffer_format_list);
        dlog(1, "done.\n");

        // evp->send.src is where we drop messages to send...
        // Example:  EVsubmit(evp->source, &msg, NULL);
        SOS_buffer *buffer;
        SOS_buffer_init_sized_locking(SOS, &buffer, 2048, false);

        SOS_msg_header header;
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = SOSD.sos_context->config.comm_rank;
        header.ref_guid = 0;

        int msg_count = 1;

        int offset = 0;
        SOS_buffer_pack(buffer, &offset, "iiigg",
                msg_count,
                header.msg_size,
                header.msg_type,
                header.msg_from,
                header.ref_guid);

        SOS_buffer_pack(buffer, &offset, "s", evp->recv.contact_string);

        header.msg_size = offset;
        offset = 0;

        SOS_buffer_pack(buffer, &offset, "ii",
                msg_count,
                header.msg_size);

        SOSD_cloud_send(buffer, NULL);
        SOS_buffer_destroy(buffer);
    }
    
    FILE *present_file;
    // set the node id before we use it.
    present_file = fopen(present_filename, "w");
    fprintf(present_file, "%s\n%s\n%s\n",
            evp->recv.contact_string,
            SOSD.sos_context->config.node_id,
            SOSD.net->local_port);
    fflush(present_file);
    fclose(present_file);


    free(contact_filename);
    free(present_filename);
    dlog(1, "   ... done.\n");

    return 0;
}


/* name.......: SOSD_cloud_start
 * description: In the event that initialization and activation are not
 *    necessarily the same, when this function returns the communication
 *    between sosd instances is active, and all cloud functions are
 *    operating.
 */
int SOSD_cloud_start(void) {
    //NOTE: Presently unused for EVPath.
    return 0;
}



/* name.......: SOSD_cloud_send
 * description: Send a message to the target aggregator.
 *              NOTE: For EVPath, the reply buffer is not used,
 *              since it has an async communication model.
 */
int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send.EVPATH");

    if (reply == NULL) {
        // NOTE: This is valid behavior, so be careful to account
        //       for it.
    }

    buffer_rec rec;
    rec.data = (unsigned char *) buffer->data;
    rec.size = buffer->len;
    EVsubmit(SOSD.daemon.evpath.send.src, &rec, NULL);

    

    return 0;
}


/* name.......: SOSD_cloud_enqueue
 * description: Accept a message into the async send-queue.  (non-blocking)
 *              The purpose of this abstraction is to eventually allow
 *              SOSD to manage the bundling of multiple messages before
 *              passing them off to the underlying transport API. In the
 *              case of fine-grained messaging layers like EVPath, this
 *              is likely overkill, but nevertheless, here it is.
 */
void  SOSD_cloud_enqueue(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_enqueue.EVPATH");
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
            &header.ref_guid);

    dlog(6, "Enqueueing a %s message of %d bytes...\n",
            SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_size);
    if (buffer->len != header.msg_size) { dlog(1, "  ... ERROR:"
            " buffer->len(%d) != header.msg_size(%d)",
            buffer->len, header.msg_size); }

    pthread_mutex_lock(SOSD.sync.cloud_send.queue->sync_lock);
    pipe_push(SOSD.sync.cloud_send.queue->intake, (void *) &buffer, sizeof(SOS_buffer *));
    SOSD.sync.cloud_send.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.cloud_send.queue->sync_lock);

    dlog(1, "  ... done.\n");
    return;
}


// name.......: SOSD_cloud_fflush
// description: Force the send-queue to flush and transmit.
// note.......: With EVPath, this might be totally unnecessary.
//              (i.e. "Let EVPath handle it...")
//
void  SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush.EVPATH");

    // NOTE: This not used with EVPath.

    return;
}


/* name.......: SOSD_cloud_finalize
 * description: Shut down the cloud operation, flush / close files, etc.
 */
int   SOSD_cloud_finalize(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_finalize.EVPATH");

    SOSD_evpath *evp = &SOSD.daemon.evpath;

    if (SOSD.sos_context->role != SOS_ROLE_AGGREGATOR) {
        return 0;
    }
    char *contact_filename = (char *) calloc(2048, sizeof(char));
    snprintf(contact_filename, 2048, "%s/sosd.%05d.key",
            evp->meetup_path, SOS->config.comm_rank);
    dlog(1, "   Removing key file: %s\n", contact_filename);

    if (remove(contact_filename) == -1) {
        dlog(0, "   Error, unable to delete key file!\n");
    }

    return 0;
}


/* name.......: SOSD_cloud_shutdown_notice
 * description: Send notifications to any daemon ranks that are not in the
 *              business of listening to the node on the SOS_CMD_PORT socket.
 *              Only certain daemon ranks participate/call this function.
 */
void  SOSD_cloud_shutdown_notice(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_shutdown_notice.EVPATH");

    SOS_buffer *shutdown_msg;
    SOS_buffer_init(SOS, &shutdown_msg);

    dlog(1, "Providing shutdown notice to the cloud_sync backend...\n");
    SOSD_cloud_shutdown_underway = true;

    if ((SOS->config.comm_rank - SOSD.daemon.cloud_sync_target_count)
            < SOSD.daemon.cloud_sync_target_count)
    {
        dlog(1, "  ... preparing notice to SOS_ROLE_AGGREGATOR at"
                " rank %d\n", SOSD.daemon.cloud_sync_target);
        // LISTENER:
        // The first N listener ranks will notify the N aggregators...
        //
        SOS_msg_header header;
        SOS_buffer    *shutdown_msg;
        SOS_buffer    *reply;
        int            embedded_msg_count;
        int            offset;
        int            msg_inset;

        SOS_buffer_init(SOS, &shutdown_msg);
        SOS_buffer_init_sized_locking(SOS, &reply, 10, false);

        embedded_msg_count = 1;
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
        header.msg_from = SOS->my_guid;
        header.ref_guid = 0;

        offset = 0;
        SOS_buffer_pack(shutdown_msg, &offset, "i", embedded_msg_count);
        msg_inset = offset;


        header.msg_size = SOS_buffer_pack(shutdown_msg, &offset, "iigg",
                header.msg_size,
                header.msg_type,
                header.msg_from,
                header.ref_guid);
        offset = 0;
        SOS_buffer_pack(shutdown_msg, &offset, "ii",
                embedded_msg_count,
                header.msg_size);

        dlog(1, "  ... sending notice\n");
        SOSD_cloud_send(shutdown_msg, reply);
        dlog(1, "  ... sent successfully\n");

        SOS_buffer_destroy(shutdown_msg);
        SOS_buffer_destroy(reply);

    } else if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {
        // AGGREGATOR:
        //     Build a dummy message to send to ourself to purge the
        //     main listen loop's accept() call:
        dlog(1, "Processing a self-send to flush main listen"
                " loop's socket accept()...\n");

        SOS_msg_header header;
        int offset;

        SOS_buffer *shutdown_msg = NULL;
        SOS_buffer *shutdown_reply = NULL;
        SOS_buffer_init(SOS, &shutdown_msg);
        SOS_buffer_init(SOS, &shutdown_reply);

        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
        header.msg_from = SOS->my_guid;
        header.ref_guid = -1;

        offset = 0;
        SOS_msg_zip(shutdown_msg, header, 0, &offset);

        header.msg_size = offset;
        offset = 0;
        SOS_msg_zip(shutdown_msg, header, 0, &offset);

        SOSD_send_to_self(shutdown_msg, shutdown_reply);
        SOS_buffer_destroy(shutdown_msg);
        SOS_buffer_destroy(shutdown_reply);
    }

    dlog(1, "  ... done\n");

    return;
}


/* name.......: SOSD_cloud_listen_loop
 * description: When there is a feedback/control mechanism in place
 *              between the daemons and a heirarchical authority / policy
 *              enactment chain, this will be the loop that is monitoring
 *              incoming messages from other sosd daemon instances.
 */
void  SOSD_cloud_listen_loop(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop.EVPATH");

    // NOTE: This work is handled by EVPath's message handler.
    dlog(2, "Entering cloud listening loop...\n");
    while(!SOSD_evpath_ready_to_listen) {
        usleep(50000);
    }
    dlog(2, "Leaving cloud listening loop.\n");

    return;
}


