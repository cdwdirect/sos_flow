#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <mpi.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_cloud_mpi.h"
#include "sosd_db_sqlite.h"
#include "sos_types.h"
#include "sos_buffer.h"
#include "sos_qhashtbl.h"
#include "sos_pipe.h"
#include "sos_target.h"

pthread_t *SOSD_cloud_flush;
bool SOSD_cloud_shutdown_underway;

void SOSD_cloud_shutdown_notice(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_shutdown_notice");
    /* One rank receives the sosd.kill and send the SHUTDOWN to all
     * the others mpi ranks.
     */

    SOS_buffer *shutdown_msg;
    SOS_buffer_init(SOS, &shutdown_msg);
    int world_size = -1;
    int world_rank = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    dlog(1, "Providing shutdown notice to the cloud_sync backend...\n");
    SOSD_cloud_shutdown_underway = true;


    SOS_msg_header header;
    int            offset = 0;
    int            msg_inset;

    SOS_buffer_init(SOS, &shutdown_msg);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

    header.msg_size = SOS_msg_zip(shutdown_msg, header, offset, &offset);
    offset = 0;
    SOS_buffer_pack(shutdown_msg, &offset, "i",
                    header.msg_size);

    dlog(1, "  ... sending shutdown notice\n");
    int i;
    for(i=0; i<world_rank; i++)
    {
        dlog(1, "  ... preparing notice to daemon at rank %d\n", i);
        MPI_Ssend((void *) shutdown_msg->data, shutdown_msg->len, MPI_CHAR, i, 0, MPI_COMM_WORLD);
        dlog(1, "  ... sent successfully\n");
    }
    for(i=(world_rank+1); i<world_size; i++)
    {
        dlog(1, "  ... preparing notice to daemon at rank %d\n", i);
        MPI_Ssend((void *) shutdown_msg->data, shutdown_msg->len, MPI_CHAR, i, 0, MPI_COMM_WORLD);
        dlog(1, "  ... sent successfully\n");
    }


    dlog(1, "  ... done\n");

    SOS_buffer_destroy(shutdown_msg);

    return;
}




void SOSD_cloud_enqueue(SOS_buffer *buffer) {
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
                      &header.ref_guid);

    dlog(6, "Enqueueing a %s message of %d bytes...\n", SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_size);
    if (buffer->len != header.msg_size) { dlog(1, "  ... ERROR: buffer->len(%d) != header.msg_size(%d)", buffer->len, header.msg_size); }

    pthread_mutex_lock(SOSD.sync.cloud_send.queue->sync_lock);
    pipe_push(SOSD.sync.cloud_send.queue->intake, (void *) &buffer, sizeof(SOS_buffer *));
    SOSD.sync.cloud_send.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.cloud_send.queue->sync_lock);

    dlog(1, "  ... done.\n");
    return;
}


void SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush");
    dlog(5, "Signaling SOSD.sync.cloud_send.queue->sync_cond ...\n");
    if (SOSD.sync.cloud_send.queue != NULL) {
        pthread_cond_signal(SOSD.sync.cloud_send.queue->sync_cond);
    }
    return;
}


int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   offset = 0;
    int   rc;

    dlog(8, "-----------> ----> -------------> ----------> ------------->\n");
    dlog(8, "----> --> >>Transporting off-node!>> ---------------------->\n");
    dlog(8, "---------------> ---------> --------------> ----> -----> -->\n");

    /* At this point, it's pretty simple: */
    MPI_Ssend((void *) buffer->data, buffer->len, MPI_CHAR, SOSD.daemon.cloud_sync_target, 0, MPI_COMM_WORLD);

    SOSD_countof(mpi_sends++);
    SOSD_countof(mpi_bytes += buffer->len);

    /* NOTE: buffer gets destroyed by the calling function. */
    return 0;
}



void SOSD_cloud_listen_loop(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop(MPI)");
    MPI_Status      status;
    SOS_msg_header  header;
    SOS_buffer     *buffer;
    SOS_buffer     *reply;
    SOS_buffer     *query_results;
    char            unpack_format[SOS_DEFAULT_STRING_LEN] = {0};
    int             msg_waiting;
    int             mpi_msg_len;
    int             msg_offset;
    dlog(8, "SOSD_cloud_listen_loop...\n");
    SOS_buffer_init(SOS, &buffer);
    SOS_buffer_init_sized_locking(SOS, &reply, SOS_DEFAULT_BUFFER_MAX, false);
    dlog(8, "SOSD_cloud_listen_loop, entering loop SOSD.daemon.running %d...\n", SOSD.daemon.running);

    while(!SOSD.daemon.running) {
        usleep(1000);
    }
    dlog(8, "SOSD_cloud_listen_loop, entering loop SOSD.daemon.running %d...\n", SOSD.daemon.running);
    while(SOSD.daemon.running) {
        /* Receive a composite message from a daemon: */
        dlog(5, "Waiting for a message from MPI...\n");

        msg_waiting = 0;
        do {
            //If the daemon stop running while waiting for another message, stop listening.
            if(!SOSD.daemon.running)
            {
                dlog(1,"SOSD.daemon.running is 0, exit SOSD_cloud_listen_loop");
                return;
            }
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_waiting, &status);
            usleep(1000);
        } while (msg_waiting == 0);

        MPI_Get_count(&status, MPI_CHAR, &mpi_msg_len);

        while(buffer->max < mpi_msg_len) {
            SOS_buffer_grow(buffer, (1 + (mpi_msg_len - buffer->max)), SOS_WHOAMI);
        }

        MPI_Recv((void *) buffer->data, mpi_msg_len, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
        dlog(1, "  ... message of %d bytes received from rank %d!\n", mpi_msg_len, status.MPI_SOURCE);

        int displaced     = 0;
        int offset        = 0;

        /* Extract one single messages into 'msg' */
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

            dlog(1, "[ccc] <<< bringing in msg(%15s).size == %d from offset:%d\n",
                 SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE),
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

            case SOS_MSG_TYPE_REGISTER:
                dlog(1, "Received 'SOS_MSG_TYPE_REGISTER' which is not needed for MPI configurations.");
                break;

            case SOS_MSG_TYPE_GUID_BLOCK:
                // Discard the message, all that matters is that it is a GUID request.
                SOS_buffer_destroy(msg);
                // Re-use the pointer variable to assemble a response.
                SOS_buffer_init_sized_locking(SOS, &msg, (1 + (sizeof(SOS_guid) * 2)), false);
                // Break off some GUIDs for this request.
                SOS_guid guid_from = 0;
                SOS_guid guid_to   = 0;
                SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &guid_from, &guid_to);
                // Pack them into a reply.   (No message header needed)
                offset = 0;
                SOS_buffer_pack(msg, &offset, "gg",
                                guid_from,
                                guid_to);
                // Send GUIDs back to the requesting (likely analytics) rank.
                MPI_Ssend((void *) msg->data, msg->len, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                // Clean up
                SOS_buffer_destroy(msg);
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

            default:                      SOSD_handle_unknown    (msg); break;
            }
        }
    //}
    SOS_buffer_destroy(buffer);

    return;
}


// NOTE: Trigger pulls do not flow out beyond the node where
//       they are pulled (at this time).  They go "downstream"
//       from AGGREGATOR->LISTENER and LISTENER->LOCALAPPS
void SOSD_cloud_handle_triggerpull(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_cloud_handle_triggerpull(MPI)");
    dlog(4, "Message received... unzipping.\n");

    SOS_msg_header header;
    int offset = 0;
    SOS_msg_unzip(msg, &header, 0, &offset);

    int offset_after_original_header = offset;

    dlog(4, "Done unzipping.  offset_after_original_header == %d\n",
            offset_after_original_header);

    if ((SOS->role == SOS_ROLE_AGGREGATOR)
     && (SOS->config.comm_size > 1)) {

        dlog(4, "I am an aggregator, and I have some"
                " listener[s] to notify.\n");

        dlog(2, "Wrapping the trigger message...\n");

        SOS_buffer *wrapped_msg;
        SOS_buffer_init_sized_locking(SOS, &wrapped_msg, (msg->len + 4 + 1), false);

        header.msg_size = msg->len;
        header.msg_type = SOS_MSG_TYPE_TRIGGERPULL;
        header.msg_from = SOS->config.comm_rank;
        header.ref_guid = 0;

        offset = 0;
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
        SOS_buffer_pack(wrapped_msg, &offset, "i",
            header.msg_size);

        int id;
        for (id = SOSD.daemon.cloud_sync_target_count; id < SOS->config.comm_size; id++) {
            //Check which aggregator should send to this id and check if
            //it is this aggregator
            int my_listener = id % SOSD.daemon.cloud_sync_target_count;
            my_listener = ( my_listener == SOS->config.comm_rank ) ? 1:0;
            if(my_listener)
                MPI_Ssend((void *) wrapped_msg->data, wrapped_msg->len, MPI_CHAR, id, 0, MPI_COMM_WORLD);
            
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

int SOSD_cloud_init(int *argc, char ***argv) {

    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;
    int   this_node;

    int   cloud_sync_target_count;

    SOSD.sos_context->config.comm_rank = -999;
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_init");
    dlog(8, "SOSD_cloud_init...\n");


    SOSD_cloud_shutdown_underway = false;

    if (SOSD_ECHO_TO_STDOUT) printf("Configuring this daemon with MPI:\n");
    if (SOSD_ECHO_TO_STDOUT) printf("  ... calling MPI_Init_thread();\n");

    SOS->config.comm_support = -1;
    rc = MPI_Init_thread( argc, argv, MPI_THREAD_MULTIPLE, &SOS->config.comm_support );
    if (rc != MPI_SUCCESS) {
        MPI_Error_string( rc, mpi_err, &mpi_err_len );
        printf("  ... MPI_Init_thread() did not complete successfully!\n");
        printf("  ... Error %d: %s\n", rc, mpi_err);
        exit( EXIT_FAILURE );
    }
    if (SOSD_ECHO_TO_STDOUT) printf("  ... safely returned.\n");

    int world_size = -1;
    int world_rank = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    if (world_rank == 0) {
        switch (SOS->config.comm_support) {
        case MPI_THREAD_SINGLE:     printf("  MPI supported: MPI_THREAD_SINGLE (could cause problems w/any MPI version)\n"); break; 
        case MPI_THREAD_FUNNELED:   printf("  MPI supported: MPI_THREAD_FUNNELED (could cause problems w/any MPI version)\n"); break;
        case MPI_THREAD_SERIALIZED: printf("  MPI supported: MPI_THREAD_SERIALIZED (could cause problems w/MVAPICH)\n"); break;
        case MPI_THREAD_MULTIPLE:   printf("  MPI supported: MPI_THREAD_MULTIPLE (OK!)\n"); break;
        default: printf("  ... WARNING!  The supported threading model (%d) is unrecognized!\n",
                   SOS->config.comm_support); break;
        }
        fflush(stdout);
    }

    dlog(0,"Aggregator_count = %d, listener_count = %d\n", SOSD.daemon.aggregator_count, SOSD.daemon.listener_count);
    if ((world_rank >= SOSD.daemon.aggregator_count) && (world_rank <= world_size)) {
        SOS->role = SOS_ROLE_LISTENER;
        dlog(0, "Becoming a SOS_ROLE_LISTENER...world_rank %d...\n", world_rank);
    }
    else if ((world_rank >= 0) && (world_rank < SOSD.daemon.aggregator_count)) { 
        SOS->role = SOS_ROLE_AGGREGATOR;
        dlog(0, "Becoming a SOS_ROLE_AGGREGATOR....world_rank %d...\n", world_rank);
    } 
    
    if (SOS->role == SOS_ROLE_UNASSIGNED) {
        dlog(0, "ERROR: Unable to determine a role for this instance of SOSD!\n");
        dlog(0, "ERROR: Verify the number of ranks matches the reqested roles.\n");
        exit(EXIT_FAILURE);
    }

    dlog(1,"Rank(%d)Size(%d)Role(%d) ==> list:%d    aggr:%d    work:%s    port:%d\n",
            world_rank, world_size, SOS->role,
            SOSD.daemon.listener_count, SOSD.daemon.aggregator_count,
            SOSD.daemon.work_dir, SOSD.net->port_number);
    fflush(stdout);

    // ----- Setup the cloud_sync target: ----------
    if (SOSD_ECHO_TO_STDOUT) printf("Broadcasting world roles and host names...\n");

    // Information about this rank.
    int   my_role;
    char *my_host;
    int   my_host_name_len;

    // Target arrays for MPI_Allgather of roles and host names.
    int  *world_roles;
    char *world_hosts;

    // WORLD DISCOVER: ----------
    //   (includes ANALYTICS ranks)
    my_host     = (char *) calloc(MPI_MAX_PROCESSOR_NAME, sizeof(char));
    world_hosts = (char *) calloc(world_size * (MPI_MAX_PROCESSOR_NAME), sizeof(char));
    world_roles =  (int *) calloc(world_size, sizeof(int));
    my_role = SOS->role;
    MPI_Get_processor_name(my_host, &my_host_name_len);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... sending SOS->role   (%s)\n", SOS_ENUM_STR(SOS->role, SOS_ROLE));
    MPI_Allgather((void *) &my_role, 1, MPI_INT, world_roles, 1, MPI_INT, MPI_COMM_WORLD);
    MPI_Allgather((void *) my_host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
                  (void *) world_hosts, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, MPI_COMM_WORLD);

    MPI_Comm_rank(MPI_COMM_WORLD, &SOS->config.comm_rank );
    MPI_Comm_size(MPI_COMM_WORLD, &SOS->config.comm_size );
    if (SOSD_ECHO_TO_STDOUT) printf("  ... rank: %d\n", SOS->config.comm_rank);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... size: %d\n", SOS->config.comm_size);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");

    // Count the SOS_ROLE_AGGREGATOR's to find out how large our list need be...
    cloud_sync_target_count = 0;
    for (this_node = 0; this_node < SOS->config.comm_size; this_node++) {
        if (world_roles[this_node] == SOS_ROLE_AGGREGATOR) { cloud_sync_target_count++; }
    }
    if (cloud_sync_target_count == 0) {
        printf("ERROR!  No daemon's are set to receive cloud_sync messages!\n");
        exit(EXIT_FAILURE);
    }
    SOSD.daemon.cloud_sync_target_set = (int *) malloc(cloud_sync_target_count * sizeof(int));
    memset(SOSD.daemon.cloud_sync_target_set, '\0', (cloud_sync_target_count * sizeof(int)));
    // Compile the list of the SOS_ROLE_AGGREGATOR's ...
    cloud_sync_target_count = 0;
    for (this_node = 0; this_node < SOS->config.comm_size; this_node++) {
        if (world_roles[this_node] == SOS_ROLE_AGGREGATOR) {
            SOSD.daemon.cloud_sync_target_set[cloud_sync_target_count] = this_node;
            cloud_sync_target_count++;
        }
    }
    SOSD.daemon.cloud_sync_target_count = cloud_sync_target_count;

    // Select the SOS_ROLE_AGGREGATOR we're going to cloud_sync with...
    if (SOS->config.comm_rank > 0) {
        SOSD.daemon.cloud_sync_target =                                 \
            SOSD.daemon.cloud_sync_target_set[SOS->config.comm_rank % SOSD.daemon.cloud_sync_target_count];
    } else {
        SOSD.daemon.cloud_sync_target = SOSD.daemon.cloud_sync_target_set[0];
    }
    if (SOSD_ECHO_TO_STDOUT) printf("  ... SOSD.daemon.cloud_sync_target == %d\n", SOSD.daemon.cloud_sync_target);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");
    // -------------------- 

    free(my_host);
    free(world_hosts);
    free(world_roles);

    return 0;
}


int SOSD_cloud_start() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_start");
    int rc;

    /*
     *  This is handled elsewhere.
     *
    if (SOS->role != SOS_ROLE_AGGREGATOR) {
        if (SOSD_ECHO_TO_STDOUT) printf("Launching cloud_sync flush/p thread...\n");
        SOSD.sync.cloud_send.handler = (pthread_t *) malloc(sizeof(pthread_t));
        rc = pthread_create(SOSD.sync.cloud_send.handler, NULL, (void *) SOSD_THREAD_cloud_flush, (void *) &SOSD.sync.cloud_send.;
        if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");
    }
    */

    pthread_cond_signal(SOSD.sync.cloud_send.queue->sync_cond);

    return 0;
}


int SOSD_cloud_finalize(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_finalize(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;

    dlog(1, "Shutting down SOSD cloud services...\n");
    dlog(1, "  ... forcing the cloud_sync buffer to flush.  (flush thread exits)\n");
    SOSD_cloud_fflush();
    dlog(1, "  ... joining the cloud_sync flush thread.\n");
    if (SOSD.sync.cloud_send.handler != NULL) {
        pthread_join(*SOSD.sync.cloud_send.handler, NULL);
        free(SOSD.sync.cloud_send.handler);
    }

    dlog(1, "  ... cleaning up the cloud_sync_set list.\n");
    memset(SOSD.daemon.cloud_sync_target_set, '\0', (SOSD.daemon.cloud_sync_target_count * sizeof(int)));
    free(SOSD.daemon.cloud_sync_target_set);

    dlog(1, "Leaving the MPI communicator...\n");
    MPI_Barrier(MPI_COMM_WORLD);
    rc = MPI_Finalize();

    if (rc != MPI_SUCCESS) {
        MPI_Error_string( rc, mpi_err, &mpi_err_len );
        dlog(1, "  ... MPI_Finalize() did not complete successfully!\n");
        dlog(1, "  ... Error %d: %s\n", rc, mpi_err);
    }
    dlog(1, "  ... Clearing the SOS->config.comm_* fields.\n");
    SOS->config.comm_rank    = -1;
    SOS->config.comm_size    = -1;
    SOS->config.comm_support = -1;
    dlog(1, "  ... done.\n");

    return 0;
}
