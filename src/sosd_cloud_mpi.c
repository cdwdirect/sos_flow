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

pthread_t *SOSD_cloud_flush;
bool SOSD_cloud_shutdown_underway;

void SOSD_cloud_shutdown_notice(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_shutdown_notice");
    /* NOTE: This function is to facilitate notification of sosd components
     *       that might not be listening to a socket.
     *       Only certain ranks will participate in it.
     */

    SOS_buffer *shutdown_msg;
    SOS_buffer_init(SOS, &shutdown_msg);

    dlog(1, "Providing shutdown notice to the cloud_sync backend...\n");
    SOSD_cloud_shutdown_underway = true;

    if (SOS->config.comm_rank < SOSD.daemon.cloud_sync_target_count) {
        dlog(1, "  ... preparing notice to SOS_ROLE_AGGREGATOR at rank %d\n", SOSD.daemon.cloud_sync_target);
        /* The first N ranks will notify the N databases... */
        SOS_msg_header header;
        SOS_buffer    *shutdown_msg;
        int            embedded_msg_count;
        int            offset;
        int            msg_inset;

        SOS_buffer_init(SOS, &shutdown_msg);

        embedded_msg_count = 1;
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
        header.msg_from = SOS->my_guid;
        header.pub_guid = 0;

        offset = 0;
        SOS_buffer_pack(shutdown_msg, &offset, "i", embedded_msg_count);
        msg_inset = offset;

        header.msg_size = SOS_buffer_pack(shutdown_msg, &offset, "iigg",
                                          header.msg_size,
                                          header.msg_type,
                                          header.msg_from,
                                          header.pub_guid);
        offset = 0;
        SOS_buffer_pack(shutdown_msg, &offset, "ii",
                        embedded_msg_count,
                        header.msg_size);

        dlog(1, "  ... sending notice\n");
        MPI_Ssend((void *) shutdown_msg->data, shutdown_msg->len, MPI_CHAR, SOSD.daemon.cloud_sync_target, 0, MPI_COMM_WORLD);
        dlog(1, "  ... sent successfully\n");

    }
    
    //dlog(1, "  ... waiting at barrier for the rest of the sosd daemons\n");
    //MPI_Barrier(MPI_COMM_WORLD);
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
                      &header.pub_guid);

    dlog(6, "Enqueueing a %s message of %d bytes...\n", SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_size);
    if (buffer->len != header.msg_size) { dlog(1, "  ... ERROR: buffer->len(%d) != header.msg_size(%d)", buffer->len, header.msg_size); }

    pthread_mutex_lock(SOSD.sync.cloud.queue->sync_lock);
    pipe_push(SOSD.sync.cloud.queue->intake, (void *) &buffer, sizeof(SOS_buffer *));
    SOSD.sync.cloud.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.cloud.queue->sync_lock);

    dlog(1, "  ... done.\n");
    return;
}


void SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush");
    dlog(5, "Signaling SOSD.sync.cloud.queue->sync_cond ...\n");
    if (SOSD.sync.cloud.queue != NULL) {
        pthread_cond_signal(SOSD.sync.cloud.queue->sync_cond);
    }
    return;
}


int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   offset;
    int   rc;
    int   entry_count;

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "i", &entry_count);

    dlog(5, "-----------> ----> -------------> ----------> ------------->\n");
    dlog(5, "----> --> >>Transporting off-node!>> ---(%d entries)---->\n", entry_count);
    dlog(5, "---------------> ---------> --------------> ----> -----> -->\n");

    /* At this point, it's pretty simple: */
    MPI_Ssend((void *) buffer->data, buffer->len, MPI_CHAR, SOSD.daemon.cloud_sync_target, 0, MPI_COMM_WORLD);

    SOSD_countof(mpi_sends++);
    SOSD_countof(mpi_bytes += buffer->len);

    MPI_Status status;
    int mpi_msg_len = 0;
    int msg_waiting = 0;
    do {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_waiting, &status);
        usleep(1000);
    } while (msg_waiting == 0);
    
    MPI_Get_count(&status, MPI_CHAR, &mpi_msg_len);
    
    while(reply->max < mpi_msg_len) {
        SOS_buffer_grow(reply, (1 + (mpi_msg_len - reply->max)), SOS_WHOAMI);
    }
    
    MPI_Recv((void *) reply->data, mpi_msg_len, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
    dlog(5, "  ... message of %d bytes reply from rank %d!\n", mpi_msg_len, status.MPI_SOURCE);    

    /* NOTE: buffer gets destroyed by the calling function. */
    return 0;
}



void SOSD_cloud_listen_loop(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop(MPI)");
    MPI_Status      status;
    SOS_msg_header  header;
    SOS_buffer     *buffer;
    SOS_buffer     *msg;
    SOS_buffer     *reply;
    SOS_buffer     *query_results;
    char            unpack_format[SOS_DEFAULT_STRING_LEN] = {0};
    int             msg_waiting;
    int             mpi_msg_len;
    int             offset;
    int             msg_offset;
    int             entry_count;
    int             entry;

    SOS_buffer_init(SOS, &buffer);
    SOS_buffer_init_sized_locking(SOS, &reply, SOS_DEFAULT_BUFFER_MAX, false);
    

    while(SOSD.daemon.running) {
        entry_count = 0;
        offset = 0;

        /* Receive a composite message from a daemon: */
        dlog(5, "Waiting for a message from MPI...\n");

        msg_waiting = 0;
        do {
            MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_waiting, &status);
            usleep(1000);
        } while (msg_waiting == 0);

        MPI_Get_count(&status, MPI_CHAR, &mpi_msg_len);

        while(buffer->max < mpi_msg_len) {
            SOS_buffer_grow(buffer, (1 + (mpi_msg_len - buffer->max)), SOS_WHOAMI);
        }

        MPI_Recv((void *) buffer->data, mpi_msg_len, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
        dlog(1, "  ... message of %d bytes received from rank %d!\n", mpi_msg_len, status.MPI_SOURCE);


        offset = 0;
        SOS_buffer_unpack(buffer, &offset, "i", &entry_count);
        dlog(1, "  ... message contains %d entries.\n", entry_count);

        int displaced = 0;

        /* Extract one-at-a-time single messages into 'msg' */
        for (entry = 0; entry < entry_count; entry++) {
            dlog(1, "[ccc] ... processing entry %d of %d @ offset == %d \n", (entry + 1), entry_count, offset);
            memset(&header, '\0', sizeof(SOS_msg_header));
            displaced = SOS_buffer_unpack(buffer, &offset, "iigg",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.pub_guid);
            dlog(1, "     ... header.msg_size == %d\n", header.msg_size);
            dlog(1, "     ... header.msg_type == %s  (%d)\n", SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_type);
            dlog(1, "     ... header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
            dlog(1, "     ... header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);

            offset -= displaced;

            //Create a new message buffer:
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
                /*
                 *  TODO: { CLOUD, REPLY, FEEDBACK }
                 *
                 */
                MPI_Send((void *) reply->data, reply->len, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                /*
                 *
                 */
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


            case SOS_MSG_TYPE_QUERY:
                // Allocate space for a response.
                SOS_buffer_init_sized_locking(SOS, &query_results, 2048, false);
                // Service the query w/locking.  (Packs the results in 'response' ready to send.)
                SOSD_db_handle_sosa_query(msg, query_results);
                // Send the results back to the asking analytics rank.
                MPI_Ssend((void *) query_results->data, query_results->len, MPI_CHAR, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                // Clean up our buffers.
                SOS_buffer_destroy(msg);
                SOS_buffer_destroy(query_results);
                break;

            case SOS_MSG_TYPE_SHUTDOWN:   SOSD.daemon.running = 0;
            default:                      SOSD_handle_unknown    (msg); break;
            }
        }
    }
    SOS_buffer_destroy(buffer);

    /* Join with the daemon's and close out together... */
    //MPI_Barrier(MPI_COMM_WORLD);

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

    SOSD_cloud_shutdown_underway = false;

    if (SOSD_ECHO_TO_STDOUT) printf("Configuring this daemon with MPI:\n");
    if (SOSD_ECHO_TO_STDOUT) printf("  ... calling MPI_Init_thread();\n");

    SOS->config.comm_support = -1;
    rc = MPI_Init_thread( argc, argv, MPI_THREAD_SERIALIZED, &SOS->config.comm_support );
    if (rc != MPI_SUCCESS) {
        MPI_Error_string( rc, mpi_err, &mpi_err_len );
        printf("  ... MPI_Init_thread() did not complete successfully!\n");
        printf("  ... Error %d: %s\n", rc, mpi_err);
        exit( EXIT_FAILURE );
    }
    if (SOSD_ECHO_TO_STDOUT) printf("  ... safely returned.\n");

    switch (SOS->config.comm_support) {
    case MPI_THREAD_SINGLE:
        if (SOSD_ECHO_TO_STDOUT) 
            printf("  ... supported: MPI_THREAD_SINGLE (could cause problems)\n");
        break; 
    case MPI_THREAD_FUNNELED:
        if (SOSD_ECHO_TO_STDOUT)  
            printf("  ... supported: MPI_THREAD_FUNNELED (could cause problems)\n");
        break;
    case MPI_THREAD_SERIALIZED: 
        if (SOSD_ECHO_TO_STDOUT) 
            printf("  ... supported: MPI_THREAD_SERIALIZED\n"); 
            break;
    case MPI_THREAD_MULTIPLE:
        if (SOSD_ECHO_TO_STDOUT) 
            printf("  ... supported: MPI_THREAD_MULTIPLE\n"); 
        break;
    default: 
        if (SOSD_ECHO_TO_STDOUT) 
            printf("  ... WARNING!  The supported threading model (%d) is unrecognized!\n",  
                   SOS->config.comm_support); 
        break;
    }

    int world_size = -1;
    int world_rank = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    //GO
    if ((world_rank >= 0) && (world_rank < SOSD.daemon.listener_count)) {
        SOS->role = SOS_ROLE_LISTENER;
        dlog(1, "Becoming a SOS_ROLE_LISTENER...\n");
    } else if ((world_rank >= SOSD.daemon.listener_count) && (world_rank <= world_size)) { 
        SOS->role = SOS_ROLE_AGGREGATOR;
        dlog(1, "Becoming a SOS_ROLE_AGGREGATOR...\n");
    } 
    
    if (SOS->role == SOS_ROLE_UNASSIGNED) {
        dlog(0, "ERROR: Unable to determine a role for this instance of SOSD!\n");
        dlog(0, "ERROR: Verify the number of ranks matches the reqested roles.\n");
        exit(EXIT_FAILURE);
    }

    printf("Rank(%d)Size(%d)Role(%d) ==>  ", world_rank, world_size, SOS->role);
    printf("list:%d    aggr:%d    work:%s    port:%d\n",
            SOSD.daemon.listener_count, SOSD.daemon.aggregator_count,
            SOSD.daemon.work_dir, SOSD.net.port_number);



    //GO
    //TODO: Self-assign the role we are to become here. 



    /* ----- Setup the cloud_sync target: ----------*/
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


    // GO
    // TODO : (Warning) This use case is deprecated. Future will have
    //        Analytics tasks run as independent applications.

    // SPLIT: -------------------
    //  When we split here, we use SOS_ROLE_LISTENER for both DAEMON and DB
    //  so they are still able to communicate with each other (if they want
    //  using a non-analytics, non-MPI_COMM_WORLD communicator.)
    //
    //  The ANALYTICS ranks form their own communicator[s].
    //
    MPI_Comm_split(MPI_COMM_WORLD, SOS_ROLE_LISTENER, world_rank, &SOSD.daemon.comm);
    //
    //  Messages are now point-to-point using MPI_COMM_WORLD.
    //

    MPI_Comm_rank(MPI_COMM_WORLD, &SOS->config.comm_rank );
    MPI_Comm_size(MPI_COMM_WORLD, &SOS->config.comm_size );
    if (SOSD_ECHO_TO_STDOUT) printf("  ... rank: %d\n", SOS->config.comm_rank);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... size: %d\n", SOS->config.comm_size);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");

    /* Count the SOS_ROLE_AGGREGATOR's to find out how large our list need be...*/
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
    /* Compile the list of the SOS_ROLE_AGGREGATOR's ...*/
    cloud_sync_target_count = 0;
    for (this_node = 0; this_node < SOS->config.comm_size; this_node++) {
        if (world_roles[this_node] == SOS_ROLE_AGGREGATOR) {
            SOSD.daemon.cloud_sync_target_set[cloud_sync_target_count] = this_node;
            cloud_sync_target_count++;
        }
    }
    SOSD.daemon.cloud_sync_target_count = cloud_sync_target_count;

    /* Select the SOS_ROLE_AGGREGATOR we're going to cloud_sync with... */
    if (SOS->config.comm_rank > 0) {
        SOSD.daemon.cloud_sync_target =                                 \
            SOSD.daemon.cloud_sync_target_set[SOS->config.comm_rank % SOSD.daemon.cloud_sync_target_count];
    } else {
        SOSD.daemon.cloud_sync_target = SOSD.daemon.cloud_sync_target_set[0];
    }
    if (SOSD_ECHO_TO_STDOUT) printf("  ... SOSD.daemon.cloud_sync_target == %d\n", SOSD.daemon.cloud_sync_target);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");
    /* -------------------- */

    free(my_host);
    free(world_hosts);
    free(world_roles);

    return 0;
}


int SOSD_cloud_start() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_start");
    int rc;

    /*
    if (SOS->role != SOS_ROLE_AGGREGATOR) {
        if (SOSD_ECHO_TO_STDOUT) printf("Launching cloud_sync flush/send thread...\n");
        SOSD.sync.cloud.handler = (pthread_t *) malloc(sizeof(pthread_t));
        rc = pthread_create(SOSD.sync.cloud.handler, NULL, (void *) SOSD_THREAD_cloud_flush, (void *) &SOSD.sync.cloud);
        if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");
    }
    */

    pthread_cond_signal(SOSD.sync.cloud.queue->sync_cond);

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
    if (SOSD.sync.cloud.handler != NULL) {
        pthread_join(*SOSD.sync.cloud.handler, NULL);
        free(SOSD.sync.cloud.handler);
    }

    dlog(1, "  ... cleaning up the cloud_sync_set list.\n");
    memset(SOSD.daemon.cloud_sync_target_set, '\0', (SOSD.daemon.cloud_sync_target_count * sizeof(int)));
    free(SOSD.daemon.cloud_sync_target_set);

    dlog(1, "Leaving the MPI communicator...\n");
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
