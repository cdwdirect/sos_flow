
#include <string.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sos_target.h"

#include "sosd_cloud_zeromq.h"
#include "czmq.h"

bool SOSD_cloud_shutdown_underway = false;
bool SOSD_ready_to_listen = false;

//TODO: This becomes a thread (or gets called by a thread).
void SOSD_cloud_listen_loop(void) {
    //NOTE: This happens VERY early, we don't use dlog()
    //      or SOS_SET_CONTEXT stuff.
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop");
    
    //wait for initialization
    while (!SOSD_ready_to_listen) {
        usleep(100000);
    }
    int rc;

    SOS_buffer *msg_buffer = NULL;
    SOS_buffer_init(SOSD.sos_context, &msg_buffer);

    SOS_buffer *ack = NULL;
    SOS_buffer_init_sized_locking(SOSD.sos_context, &ack, 2048, false);
    SOSD_PACK_ACK(ack);



    zmq_msg_t zmsg;
    zmq_msg_t zmsg_ack;

    while (SOS->status == SOS_STATUS_RUNNING) {
        dlog(1, "Listening for messages from other daemons...\n");
        SOS_buffer_wipe(msg_buffer);


        //
        //Initialize zmq empty message
        rc = zmq_msg_init (&zmsg);
        //Initialize zmsg ACK message
        zmq_msg_init_data (&zmsg_ack, ack->data, ack->len, NULL, NULL);


        dlog(1, "SOS_target_recv_msg...\n");
        //int nbytes = zmq_recv (zmq->conn_listen, msg_buffer->data, 2048, 0);
        int nbytes = zmq_msg_recv (&zmsg, zmq->conn_listen, 0);
        dlog(1, "Received nbytes: %d\n", nbytes);

        msg_buffer->len = nbytes;
        //int bytes_sent = zmq_send (zmq->conn_listen, ack->data, ack->len, 0);
        int bytes_sent = zmq_msg_send (&zmsg_ack, zmq->conn_listen, 0);
        dlog(1, "bytes_sent : %d\n", bytes_sent);

        //Copy zmsg to buffer
        SOS_buffer_init_sized_locking(SOSD.sos_context, &msg_buffer, (zmq_msg_size(&zmsg)+ 1), false);
        memcpy(msg_buffer->data, zmq_msg_data (&zmsg), zmq_msg_size(&zmsg));
        //
        dlog(1, "Message received!  Processing...\n");
        SOSD_cloud_process_buffer(msg_buffer);
    }



}


//Process a buffer containing 1 message...
void SOSD_cloud_process_buffer(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_process_buffer");

    dlog(1, "SOSD_cloud_process_buffer\n");
    SOS_msg_header    header;

    int displaced    = 0;
    int offset       = 0;

    //NOTE: Sockets only ever have a single message, like other
    //      SOS point to point communications.
    
    memset(&header, '\0', sizeof(SOS_msg_header));

    SOS_msg_unzip(buffer, &header, offset, &offset);

    dlog(1, "     ... header.msg_size == %d\n",
        header.msg_size);
    dlog(1, "     ... header.msg_type == %s  (%d)\n",
        SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_type);
    dlog(1, "     ... header.msg_from == %" SOS_GUID_FMT "\n",
        header.msg_from);
    dlog(1, "     ... header.ref_guid == %" SOS_GUID_FMT "\n",
        header.ref_guid);

    //Create a new message buffer:
    SOS_buffer *msg;
    SOS_buffer_init_sized_locking(SOS, &msg, (1 + header.msg_size), false);

    //Copy the data into the new message directly:
    memcpy(msg->data, buffer->data, header.msg_size);
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

        case SOS_MSG_TYPE_REGISTER:
            SOSD_cloud_handle_daemon_registration(msg);
            break;

        case SOS_MSG_TYPE_SHUTDOWN:
            SOSD.daemon.running = 0;
            SOSD.sos_context->status = SOS_STATUS_SHUTDOWN;
            SOS_buffer *shutdown_msg;
            SOS_buffer *shutdown_rep;
            SOS_buffer_init_sized_locking(SOS, &shutdown_msg, 1024, false);
            SOS_buffer_init_sized_locking(SOS, &shutdown_rep, 1024, false);
            int offset_shut = 0;
            SOS_buffer_pack(shutdown_msg, &offset_shut, "i", offset_shut);
            SOSD_send_to_self(shutdown_msg, shutdown_rep);
            SOS_buffer_destroy(shutdown_msg);
            SOS_buffer_destroy(shutdown_rep);
            break;

        case SOS_MSG_TYPE_TRIGGERPULL:
            SOSD_cloud_handle_triggerpull(msg);
            break;

        case SOS_MSG_TYPE_ACK:
            dlog(1, "sosd(%d) received ACK message"
                " from rank %" SOS_GUID_FMT " !\n",
                    SOSD.sos_context->config.comm_rank, header.msg_from);
            break;

        default:    SOSD_handle_unknown    (msg); break;
    }

    return;
}

void SOSD_cloud_handle_daemon_registration(SOS_buffer *msg) {
    SOS_SET_CONTEXT(SOSD.sos_context,
            "SOSD_cloud_handle_daemon_registration.ZEROMQ");

    dlog(1, "Registering a new connection...");
    

    SOS_msg_header header;
    int offset = 0;
    int rc;
    SOS_buffer_unpack(msg, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.ref_guid);


    SOS_guid listener_id = header.msg_from;
    if (header.msg_from >= SOS->config.comm_size) {
        fprintf(stderr, "ERROR: You are attempting to register a rank"
                "(%" SOS_GUID_FMT ") outside the size (%d) that you"
                " specified to the daemon at launch.",
                header.msg_from,
                SOS->config.comm_size);
        fflush(stderr);
    }

    char *remote_listen_str = NULL;

    SOS_buffer_unpack_safestr(msg, &offset, &remote_listen_str);
    zmq->node[listener_id] = calloc(1, sizeof(SOSD_zeromq_node));
    zmq->node[listener_id]->conn_tgt    = zmq->conn_request;
    //zmq->node[listener_id]->conn_tgt_str = (char *) calloc(1024, sizeof(char));
    zmq->node[listener_id]->conn_tgt_str = remote_listen_str;

    dlog(1, "Registering connection from id %d(%s)\n",
           listener_id, 
           zmq->node[listener_id]->conn_tgt_str);

    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &reply, 128, false);

    SOS_msg_header header_ack;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ACK;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;

    offset = 0;
    SOS_buffer_pack(reply, &offset, "iigg",
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.ref_guid);

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(reply, &offset, "i",
        header.msg_size);


    zmq_msg_t zmsg;
    rc = zmq_msg_init_data (&zmsg, reply->data, reply->len, NULL, NULL);
    assert (rc == 0);

    //Connect to the registered node
    rc = zmq_connect(zmq->node[listener_id]->conn_tgt,
        zmq->node[listener_id]->conn_tgt_str);
    if (rc != 0 ) 
    {
        dlog(0, "Error occurred during zmq_connect(): %s\n", 
            zmq_strerror (errno));
        return ;
    }

    //Send ACK
    dlog(1, "Sending ACK registration...");
    int bytes_sent = zmq_msg_send (&zmsg , zmq->node[listener_id]->conn_tgt, 0);
    dlog(1, "Sent nbytes: %d to registered node\n", bytes_sent);
    //Receive ACK
    dlog(1, "Receiving ACK registration...");
    int nbytes = zmq_msg_recv (&zmsg , zmq->node[listener_id]->conn_tgt, 0);
    dlog(1, "Received nbytes: %d from registered node\n", nbytes);
    //Disconnect
    rc = zmq_disconnect(zmq->node[listener_id]->conn_tgt,
        zmq->node[listener_id]->conn_tgt_str);
    if (rc != 0 ) 
    {
        dlog(0, "Error occurred during zmq_disconnect(): %s\n",
            zmq_strerror (errno));
        return ;
    }


    dlog(3, "Registration complete.\n");
    return;
}


// NOTE: Trigger pulls do not flow out beyond the node where
//       they are pulled (at this time).  They go "downstream"
//       from AGGREGATOR->LISTENER and LISTENER->LOCALAPPS
void SOSD_cloud_handle_triggerpull(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_cloud_handle_triggerpull.ZEROMQ");
    
    dlog(1, "Message received... unzipping.\n");


    SOS_msg_header header;
    int offset = 0;
    int rc;
    SOS_msg_unzip(msg, &header, 0, &offset);

    int offset_after_original_header = offset;

    dlog(1, "Done unzipping.  offset_after_original_header == %d\n", 
            offset_after_original_header);

    if ((SOS->role == SOS_ROLE_AGGREGATOR)
     && (SOS->config.comm_size > 1)) {
        
        dlog(1, "I am an aggregator, and I have some"
                " listener[s] to notify.\n");

        //SOSD_evpath *evp = &SOSD.daemon.evpath;
        buffer_rec rec;

        dlog(1, "Wrapping the trigger message...\n");

        SOS_buffer *wrapped_msg;
        SOS_buffer_init_sized_locking(SOS, &wrapped_msg, (msg->len + 4 + 1), false);
        
        header.msg_size = msg->len;
        header.msg_type = SOS_MSG_TYPE_TRIGGERPULL;
        header.msg_from = SOS->config.comm_rank;
        header.ref_guid = 0;

        offset = 0;

        SOS_buffer_grow(wrapped_msg, msg->len, SOS_WHOAMI);
        memcpy(wrapped_msg->data,
                msg->data,
                msg->len);
        wrapped_msg->len = (msg->len);
        offset = wrapped_msg->len;

        header.msg_size = offset;
        offset = 0;
        dlog(4, "Tacking on the newly wrapped message size...\n");
        dlog(4, "   header.msg_size == %d\n", header.msg_size);
        SOS_buffer_pack(wrapped_msg, &offset, "i",
            header.msg_size);

        dlog(1, "SOSD_cloud_handle_triggerpull loop \n");
        int id = 0;
        for (id = 0; id < SOS->config.comm_size; id++) {
            dlog(1, "SOSD_cloud_handle_triggerpull id %d\n", id);
            if(zmq->node[id]==NULL)
            {
                dlog(1, " skipping id %d, not registered in this aggregator\n", id);
                continue;
            }

                        
            // NOTE: See SOSD_cloud_enqueue() for async sends.
            /*dlog(1, "SOSD_cloud_handle_triggerpull send to %s:%s\n", rmt_tgt->remote_host, rmt_tgt->remote_port);
            SOS_target_connect(rmt_tgt);
            int bytes_sent = SOS_target_send_msg(rmt_tgt, wrapped_msg);
            dlog(1, "Number of bytes sent: %d\n", bytes_sent);
            SOS_target_disconnect(rmt_tgt);*/
            zmq_msg_t zmsg;
            rc = zmq_msg_init_data (&zmsg, wrapped_msg->data, wrapped_msg->len, NULL, NULL);
            assert (rc == 0);

            //Connect to the registered node
            rc = zmq_connect(zmq->node[id]->conn_tgt,
                zmq->node[id]->conn_tgt_str);
            if (rc != 0 ) 
            {
                dlog(0, "Error occurred during zmq_connect(): %s\n", 
                    zmq_strerror (errno));
                return ;
            }

            //Send ACK
            dlog(1, "Sending triggerpull...");
            int bytes_sent = zmq_msg_send (&zmsg , zmq->node[id]->conn_tgt, 0);
            dlog(1, "Sent nbytes: %d to triggerpull node\n", bytes_sent);
            //Receive ACK
            dlog(1, "Receiving ACK ...");
            int nbytes = zmq_msg_recv (&zmsg , zmq->node[id]->conn_tgt, 0);
            dlog(1, "Received nbytes: %d from triggerpull node\n", nbytes);
            //Disconnect
            rc = zmq_disconnect(zmq->node[id]->conn_tgt,
                zmq->node[id]->conn_tgt_str);
            if (rc != 0 ) 
            {
                dlog(0, "Error occurred during zmq_disconnect(): %s\n",
                    zmq_strerror (errno));
                return ;
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

    //fprintf(stderr, "sosd(%d) enquing the following task->ref:\n"
    //        "   payload->handle == %s\n"
    //        "   payload->size   == %d\n"
    //        "   payload->data   == \"%s\"\n",
    //        SOSD.sos_context->config.comm_rank,
    //        payload->handle,
    //        payload->size,
    //        (char*) payload->data);
    //fflush(stderr);

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
 *    to the centralized store (here, stone?) that this daemon is pointing to
 *    for off-node transport.  The general system allows for multiple "back-
 *    plane stores" launched alongside the daemons, to provide reasonable
 *    scalability and throughput.
 */
int SOSD_cloud_init(int *argc, char ***argv) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_init.ZEROMQ");
    //int major, minor, patch;
    //zmq_version (&major, &minor, &patch);
    //printf ("Current 0MQ version is %d.%d.%d\n", major, minor, patch);

    SOSD_ready_to_listen = false;
    zmq = &SOSD.daemon.zeromq;

    zmq->discovery_dir = NULL;
    zmq->discovery_dir = getenv("SOS_DISCOVERY_DIR");
    if ((zmq->discovery_dir == NULL) || (strlen(zmq->discovery_dir) < 1)) {
        zmq->discovery_dir = (char *) calloc(PATH_MAX, sizeof(char));
        if (!getcwd(zmq->discovery_dir, SOS_DEFAULT_STRING_LEN)) {
            fprintf(stderr, "ERROR: The SOS_MEETUP_PATH evar was not set,"
                    " and getcwd() failed! Set the evar and retry.\n");
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
        fprintf(stderr, "STATUS: The SOS_MEETUP_PATH evar was not set.\n"
                        "        Using getcwd() path: %s\n", zmq->discovery_dir);
        fflush(stderr);
    }

    int expected_node_count =
        SOSD.daemon.aggregator_count + 
        SOSD.daemon.listener_count;

    SOS->config.comm_size = expected_node_count;;
    SOS->config.comm_support = -1; // Used for MPI only.

    // Do some sanity checks.
    if (SOSD.daemon.aggregator_count == 0) {
        fprintf(stderr, "ERROR: SOS requires one or more aggregator roles.\n");
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

    SOSD.daemon.cloud_sync_target_count = SOSD.daemon.aggregator_count;

    dlog(1, "Initializing ZeroMQ...\n");

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

    dlog(1, "   ... creating connection manager:\n");


    SOSD.sos_context->config.node_id = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    gethostname( SOSD.sos_context->config.node_id, SOS_DEFAULT_STRING_LEN );

    // Do zmq_ctx_destroy() at shutdown
    zmq->context = zmq_ctx_new();
    assert (zmq->context);
    //Set the socket which other processes will send messages to
    //and then will reply using the same connection
    zmq->conn_listen = zmq_socket ( zmq->context, ZMQ_REP);
    assert(zmq->conn_listen);
    int rc = zmq_bind(zmq->conn_listen, "tcp://*:*");
    assert(rc == 0);

    //Get the endpoint of the socket to obtain the port the socket is bound to
    char *endpoint = (char *) calloc(SOS_DEFAULT_STRING_LEN, sizeof(char));
    size_t size = SOS_DEFAULT_STRING_LEN;
    rc = zmq_getsockopt( zmq->conn_listen, ZMQ_LAST_ENDPOINT, endpoint, &size );
    assert(rc == 0);

    //Read after the last ':'  to obtain the port number
    char *sosd_msg_port = (char *) calloc(SOS_DEFAULT_STRING_LEN, sizeof(char));
    char * pch;
    pch=strrchr(endpoint,':');
    strncpy(sosd_msg_port, pch+1, size);

    //Create the connection string the other processes will use to connect
    //to this process --> tcp:// + hostname + port
    zmq->conn_listen_str = (char *) calloc(SOS_DEFAULT_STRING_LEN, sizeof(char));
    snprintf(zmq->conn_listen_str, SOS_DEFAULT_STRING_LEN, "tcp://%s:%s",
        SOSD.sos_context->config.node_id, sosd_msg_port);
    dlog(1,"zmq->conn_listen_str --> %s\n", zmq->conn_listen_str);
    



    char *contact_filename = (char *) calloc(2048, sizeof(char));
    snprintf(contact_filename, 2048, "%s/sosd.%05d.key",
        zmq->discovery_dir, aggregation_rank);
    dlog(1, "   ... contact_filename: %s\n", contact_filename);

    char *present_filename = (char *) calloc(2048, sizeof(char));
    snprintf(present_filename, 2048, "%s/sosd.%05d.id",
        zmq->discovery_dir, SOSD.sos_context->config.comm_rank);
    dlog(1, "   ... present_filename: %s\n", present_filename);





    //Set the socket which this process will use to recv
    //messages from and then will reply using the same socket
    zmq->conn_request = zmq_socket ( zmq->context, ZMQ_REQ);
    assert(zmq->conn_request);



    // This is where EVPath would start listening.
    //
    // CMfork_comm_thread(evp->recv.cm);
    // TODO: Kick up the thread?
    
    // Get the location we're listening on...
    // evp->recv.contact_string =
    //     attr_list_to_string(CMget_contact_list(evp->recv.cm));

    //TODO: Write out to the .key file how other ranks can get ahold of us.



    if (SOSD.sos_context->role == SOS_ROLE_AGGREGATOR) {

        // AGGREGATOR
        //   ... the aggregator needs to wait on the registration messages
        //   before being able to create sending stones.

        dlog(0, "   ... demon role: AGGREGATOR\n");
        // Make space to track connections back to the listeners:



         zmq->node = (SOSD_zeromq_node **)
            malloc(expected_node_count * sizeof(SOSD_zeromq_node *));
        int node_idx = 0;
        for (node_idx = 0; node_idx < expected_node_count; node_idx++) {
            // Allocate space to store returning connections to clients...
            // NOTE: Fill in later, as clients connect.
            zmq->node[node_idx] = NULL;
        }

        FILE *contact_file;
		
        contact_file = fopen(contact_filename, "w");
        fprintf(contact_file, "%s\n%s\n%s\n",
                zmq->conn_listen_str,
                SOSD.sos_context->config.node_id,
                sosd_msg_port);
        fflush(contact_file);
        fclose(contact_file);

    } else {

        //LISTENER

        dlog(0, "   ... waiting for coordinator to share contact"
                " information.\n");
        while (!SOS_file_exists(contact_filename)) {
            usleep(100000);
        }

        char *agg_send_str = (char *) calloc(SOS_DEFAULT_STRING_LEN, sizeof(char));
        while(strnlen(agg_send_str, SOS_DEFAULT_STRING_LEN) < 1) {
            FILE *contact_file;
            contact_file = fopen(contact_filename, "r");
            if (contact_file < 0) {
                dlog(1, "   ... could not open contact file %s yet. (%d)\n",
                        contact_filename, contact_file);
                usleep(500000);
                continue;
            }
            int rc = 0;
            rc = fscanf(contact_file, "%1024s\n",
                   agg_send_str);
            if (strlen(agg_send_str) < 1) {
                dlog(1, "   ... could not read contact key file yet.\n");
            }
            fclose(contact_file);
            usleep(500000);
        }
        
        zmq->conn_request_str = agg_send_str;

        SOS_buffer *buffer;
        SOS_buffer_init_sized_locking(SOS, &buffer, 2048, false);

        SOS_msg_header header;
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = SOSD.sos_context->config.comm_rank;
        header.ref_guid = 0;

        int msg_count = 1;

        int offset = 0;
        SOS_buffer_pack(buffer, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.ref_guid);

        SOS_buffer_pack(buffer, &offset, "s",  zmq->conn_listen_str);

        header.msg_size = offset;
        offset = 0;
        
        SOS_buffer_pack(buffer, &offset, "i",
            header.msg_size);

        SOSD_cloud_send(buffer, NULL);
        SOS_buffer_destroy(buffer);
    }

    FILE *present_file;
    // set the node id before we use it.
    present_file = fopen(present_filename, "w");
    fprintf(present_file, "%s\n%s\n%s\n",
            zmq->conn_listen_str,
            SOSD.sos_context->config.node_id,
            sosd_msg_port);
    fflush(present_file);
    fclose(present_file);

    SOSD_ready_to_listen = true;
    free(contact_filename);
    free(present_filename);
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
    
    //TODO

    
    return 0;
}



int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send.ZEROMQ");
    //TODO: Use target API to send message.
    //This is a blocking send.  Use SOSD_cloud_enqueue for async push.
    dlog(1, "-----------> ----> -------------> ----------> ------------->\n");
    dlog(1, "----> --> >>Transporting off-node!>> ---------------------->\n");
    dlog(1, "---------------> ---------> --------------> ----> -----> -->\n");

    int rc = zmq_connect(zmq->conn_request, zmq->conn_request_str);
    if (rc != 0 ) 
    {
        printf ("Error occurred during zmq_connect(): %s\n",
            zmq_strerror (errno));
        return 1;
    }


    zmq_msg_t zmsg;
    rc = zmq_msg_init_data (&zmsg, buffer->data, buffer->len, NULL, NULL);
    assert (rc == 0);

    int bytes_sent = zmq_msg_send (&zmsg ,zmq->conn_request, 0);
    dlog(1, "bytes_sent : %d\n", bytes_sent);
    int nbytes = zmq_msg_recv (&zmsg ,zmq->conn_request, 0);
    dlog(1, "Received nbytes: %d\n", nbytes);
    rc = zmq_disconnect(zmq->conn_request, zmq->conn_request_str);
    if (rc != 0 ) 
    {
        printf ("Error occurred during zmq_disconnect(): %s\n",
            zmq_strerror (errno));
        return 1;
    }

    return 0;
}


/* name.......: SOSD_cloud_enqueue
 * description: Accept a message into the async send-queue.  (non-blocking)
 *              The purpose of this abstraction is to eventually allow
 *              SOSD to manage the bundling of multiple messages before
 *              passing them off to the underlying transport API.
 */
void  SOSD_cloud_enqueue(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_enqueue.ZEROMQ");
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
//
void  SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush.ZEROMQ");

    //TODO: Decide whether this op is supported.

    return;
}


/* name.......: SOSD_cloud_finalize
 * description: Shut down the cloud operation, flush / close files, etc.
 */
int   SOSD_cloud_finalize(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_finalize.ZEROMQ");
    dlog(1, "SOSD_cloud_finalize... Not implemented!!!.\n");
    /*
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
    */
    return 0;
}


/* name.......: SOSD_cloud_shutdown_notice
 * description: Send notifications to any daemon ranks that are not in the
 *              business of listening to the node on the SOS_CMD_PORT socket.
 *              Only certain daemon ranks participate/call this function.
 */
void  SOSD_cloud_shutdown_notice(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_shutdown_notice.ZEROMQ");

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

    } else {
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

void *SOSD_THREAD_ZEROMQ_listen_wrapper(void *not_used) {
    // Run the basic API-required loop listener function.
    // NOTE: Daemons have globals, no need to intake a parameter.
    SOSD_cloud_listen_loop();
    pthread_exit(NULL);
    return NULL;
}
