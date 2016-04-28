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
        dlog(1, "  ... preparing notice to SOS_ROLE_DB at rank %d\n", SOSD.daemon.cloud_sync_target);
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
    
    dlog(1, "  ... waiting at barrier for the rest of the sosd daemons\n");
    MPI_Barrier(MPI_COMM_WORLD);
    dlog(1, "  ... done\n");

    SOS_buffer_destroy(shutdown_msg);

    return;
}



void* SOSD_THREAD_cloud_flush(void *args) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_THREAD_cloud_flush(MPI)");
    struct timespec  tsleep;
    struct timeval   tnow;
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_pipe        *queue = my->queue;
    SOS_buffer      *send_buffer;
    SOS_buffer     **msg_list;
    int              msg_count;
    int              msg_index;
    char             embedded_msg_format[SOS_DEFAULT_STRING_LEN] = {0};
    int              offset;
    int              bytes;
    int              wake_type;

    /* Since the log files are not even open yet, we must wait to use dlog() */
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("Starting thread.\n"); }
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("  ... LOCK queue->sync_lock\n"); }
    pthread_mutex_lock(queue->sync_lock);
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("  ... COND_WAIT for daemon to finish initializing.\n"); }
    pthread_cond_wait(queue->sync_cond, queue->sync_lock);
    dlog(6, "  ... AWAKE and running now.\n");

    SOS_buffer_init(SOS, &send_buffer);

    gettimeofday(&tnow, NULL);
    tsleep.tv_sec  = tnow.tv_sec  + 0;
    tsleep.tv_nsec = (1000 * tnow.tv_usec) + 62500000;   /* ~ 0.06 seconds. */

    dlog(6, "  ... entering loop\n");
    while (SOSD.daemon.running) {
        dlog(6, "Sleeping!    (UNLOCK on queue->sync_cond)\n");
        wake_type = pthread_cond_timedwait(queue->sync_cond, queue->sync_lock, &tsleep);
        dlog(6, "Waking up!   (LOCK on queue->sync_lock)\n");

        if (SOSD_cloud_shutdown_underway) {
            dlog(6, "  ... shutdown is imminent, bailing out\n");
            break;
        }

        dlog(6, "  ... queue->elem_count == %d\n", queue->elem_count);
        if (queue->elem_count == 0) {
            dlog(6, "  ... nothing to do, going back to sleep.\n");
            gettimeofday(&tnow, NULL);
            tsleep.tv_sec  = tnow.tv_sec  + 0;
            tsleep.tv_nsec = (1000 * tnow.tv_usec) + 122500000UL;   /* ~ 0.12 seconds */
            continue;
        }

        msg_list  = (SOS_buffer **) malloc(msg_count * sizeof(SOS_buffer *));
        msg_count = pipe_pop(queue->outlet, (void *) &msg_list, queue->elem_count);
        queue->elem_count = 0;

        /* Release the lock while we build and send the MPI message, so
         * other threads are not blocked from adding to the queue while
         * we do this.
         *
         * We'll obtain the lock again before immediately going to sleep,
         * to prevent "bad things" from happening.
         */
        pthread_mutex_unlock(queue->sync_lock);

        if (wake_type == ETIMEDOUT) {
            dlog(6, "  ... timed-out\n");
        } else {
            dlog(6, "  ... manually triggered\n");
        }

        offset = 0;
        SOS_buffer_wipe(send_buffer);
        SOS_buffer_pack(send_buffer, &offset, "i", msg_count);
        
        for (msg_index = 0; msg_index < msg_count; msg_index++) {

            snprintf(embedded_msg_format, SOS_DEFAULT_STRING_LEN, "%db", msg_list[msg_index]->len);
            SOS_buffer_pack(send_buffer, &offset, embedded_msg_format,
                            msg_list[msg_index]->data);

            /* Clean up the buffer we just packed, we no longer need it. */
            SOS_buffer_destroy(msg_list[msg_index]);
        }
        free(msg_list);

        SOSD_cloud_send(send_buffer);

        /* Done.  Grab the lock again and go back to sleep. */
        pthread_mutex_lock(queue->sync_lock);
        gettimeofday(&tnow, NULL);
        tsleep.tv_sec  = tnow.tv_sec  + 5;
        tsleep.tv_nsec = tnow.tv_usec + 62500000;   /* ~ 0.06 seconds. */
    }

    dlog(6, "  ... UNLOCK queue->sync_lock\n");
    pthread_mutex_unlock(queue->sync_lock);
    dlog(1, "  ... exiting thread safely.\n");

    return NULL;
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
    pipe_push(SOSD.sync.cloud.queue->intake, (void *) buffer, sizeof(SOS_buffer *));
    SOSD.sync.cloud.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.cloud.queue->sync_lock);

    dlog(1, "  ... done.\n");
    return;
}


void SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush");
    dlog(5, "Signaling SOSD.sync.cloud.queue->sync_cond ...\n");
    pthread_cond_signal(SOSD.sync.cloud.queue->sync_cond);
    return;
}


int SOSD_cloud_send(SOS_buffer *buffer) {
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

    /* TODO: { FEEDBACK } Turn this into send/recv combo... */

    return 0;
}



void SOSD_cloud_listen_loop(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop(MPI)");
    MPI_Status      status;
    SOS_msg_header  header;
    SOS_buffer     *buffer;
    SOS_buffer     *msg;
    char            unpack_format[SOS_DEFAULT_STRING_LEN] = {0};
    int             mpi_msg_len;
    int             offset;
    int             msg_offset;
    int             entry_count;
    int             entry;

    SOS_buffer_init(SOS, &buffer);

    while(SOSD.daemon.running) {
        entry_count = 0;
        offset = 0;

        /* Receive a composite message from a daemon: */
        dlog(5, "Waiting for a message from MPI...\n");

        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_CHAR, &mpi_msg_len);

        while(buffer->max < mpi_msg_len) {
            SOS_buffer_grow(buffer);
        }

        MPI_Recv((void *) buffer->data, mpi_msg_len, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
        dlog(5, "  ... message received from rank %d!\n", status.MPI_SOURCE);
        offset = 0;
        SOS_buffer_unpack(buffer, &offset, "i", &entry_count);
        dlog(5, "  ... message contains %d entries.\n", entry_count);

        /* Extract one-at-a-time single messages into 'msg' */
        for (entry = 1; entry <= entry_count; entry++) {
            dlog(6, "  ... processing entry %d of %d @ offset == %d \n", entry, entry_count, offset);
            memset(&header, '\0', sizeof(SOS_msg_header));
            SOS_buffer_wipe(msg);
            SOS_buffer_unpack(buffer, &offset, "iigg",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.pub_guid);
            dlog(6, "     ... header.msg_size == %d\n", header.msg_size);
            dlog(6, "     ... header.msg_type == %s  (%d)\n", SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_type);
            dlog(6, "     ... header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
            dlog(6, "     ... header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);

            /* TODO: { THREAD POOL }
             *    could alloc a new message and put it in a dispatch queue
             *    for worker threads to grab and handle.
             *    ...would need to make sure __everything__ was thread safe.
             *
             *    For now, we handle it one at a time, prolly not the best.
             */

            SOS_buffer_init_sized_locking(SOS, &msg, (1 + SOS_DEFAULT_BUFFER_MIN + header.msg_size), true);

            msg_offset = 0;
            SOS_buffer_pack(msg, &msg_offset, "iigg",
                            header.msg_size,
                            header.msg_type,
                            header.msg_from,
                            header.pub_guid);

            /* Unpack the embedded message body directly into the new buffer. */
            snprintf(unpack_format, SOS_DEFAULT_STRING_LEN, "%zdb",
                     (header.msg_size - sizeof(SOS_msg_header)));
            SOS_buffer_unpack(buffer, &offset, unpack_format, (msg->data + sizeof(SOS_msg_header)));

            /* Now let's handle the message we've moved over into buffer bp-b.data */
            switch (header.msg_type) {

            case SOS_MSG_TYPE_ANNOUNCE:   SOSD_handle_announce   (msg); break;
            case SOS_MSG_TYPE_PUBLISH:    SOSD_handle_publish    (msg); break;
            case SOS_MSG_TYPE_VAL_SNAPS:  SOSD_handle_val_snaps  (msg); break;
            case SOS_MSG_TYPE_SHUTDOWN:   SOSD.daemon.running = 0;
            case SOS_MSG_TYPE_REGISTER:   /* Not for the cloud. */
            case SOS_MSG_TYPE_GUID_BLOCK: /* Not for the cloud. */
            case SOS_MSG_TYPE_ECHO:       /* Not for the cloud. */
            default:                      SOSD_handle_unknown    (msg); break;
            }

            SOS_buffer_destroy(msg);
        }
    }

    /* Join with the daemon's and close out together... */
    MPI_Barrier(MPI_COMM_WORLD);

    return;
}



int SOSD_cloud_init(int *argc, char ***argv) {
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;
    int   this_node;
    int  *my_role;
    int  *sosd_roles;
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
    case MPI_THREAD_SINGLE:     if (SOSD_ECHO_TO_STDOUT) printf("  ... supported: MPI_THREAD_SINGLE (could cause problems)\n"); break;
    case MPI_THREAD_FUNNELED:   if (SOSD_ECHO_TO_STDOUT) printf("  ... supported: MPI_THREAD_FUNNELED (could cause problems)\n"); break;
    case MPI_THREAD_SERIALIZED: if (SOSD_ECHO_TO_STDOUT) printf("  ... supported: MPI_THREAD_SERIALIZED\n"); break;
    case MPI_THREAD_MULTIPLE:   if (SOSD_ECHO_TO_STDOUT) printf("  ... supported: MPI_THREAD_MULTIPLE\n"); break;
    default:                    if (SOSD_ECHO_TO_STDOUT) printf("  ... WARNING!  The supported threading model (%d) is unrecognized!\n", SOS->config.comm_support); break;
    }
    rc = MPI_Comm_rank( MPI_COMM_WORLD, &SOS->config.comm_rank );
    rc = MPI_Comm_size( MPI_COMM_WORLD, &SOS->config.comm_size );

    if (SOSD_ECHO_TO_STDOUT) printf("  ... rank: %d\n", SOS->config.comm_rank);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... size: %d\n", SOS->config.comm_size);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");


    /* ----- Setup the cloud_sync target: ----------*/
    if (SOSD_ECHO_TO_STDOUT) printf("Broadcasting role and determining cloud_sync target...\n");
    sosd_roles = (int *) malloc(SOS->config.comm_size * sizeof(int));
    memset(sosd_roles, '\0', (SOS->config.comm_size * sizeof(int)));
    my_role = (int *) malloc(SOS->config.comm_size * sizeof(int));
    memset(my_role, '\0', (SOS->config.comm_size * sizeof(int)));
    my_role[SOS->config.comm_rank] = SOS->role;
    if (SOSD_ECHO_TO_STDOUT) printf("  ... sending SOS->role   (%s)\n", SOS_ENUM_STR(SOS->role, SOS_ROLE));
    MPI_Allreduce((void *) my_role, (void *) sosd_roles, SOS->config.comm_size, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... all roles recieved\n");
    /* Count the SOS_ROLE_DB's to find out how large our list need be...*/
    cloud_sync_target_count = 0;
    for (this_node = 0; this_node < SOS->config.comm_size; this_node++) {
        if (sosd_roles[this_node] == SOS_ROLE_DB) { cloud_sync_target_count++; }
    }
    if (cloud_sync_target_count == 0) {
        printf("ERROR!  No daemon's are set to receive cloud_sync messages!\n");
        exit(EXIT_FAILURE);
    }
    SOSD.daemon.cloud_sync_target_set = (int *) malloc(cloud_sync_target_count * sizeof(int));
    memset(SOSD.daemon.cloud_sync_target_set, '\0', (cloud_sync_target_count * sizeof(int)));
    /* Compile the list of the SOS_ROLE_DB's ...*/
    cloud_sync_target_count = 0;
    for (this_node = 0; this_node < SOS->config.comm_size; this_node++) {
        if (sosd_roles[this_node] == SOS_ROLE_DB) {
            SOSD.daemon.cloud_sync_target_set[cloud_sync_target_count] = this_node;
            cloud_sync_target_count++;
        }
    }
    SOSD.daemon.cloud_sync_target_count = cloud_sync_target_count;
    memset(my_role, '\0', (SOS->config.comm_size * sizeof(int)));
    memset(sosd_roles, '\0', (SOS->config.comm_size * sizeof(int)));
    free(my_role);
    free(sosd_roles);
    /* Select the SOS_ROLE_DB we're going to cloud_sync with... */
    if (SOS->config.comm_rank > 0) {
        SOSD.daemon.cloud_sync_target =                                 \
            SOSD.daemon.cloud_sync_target_set[SOS->config.comm_rank % SOSD.daemon.cloud_sync_target_count];
    } else {
        SOSD.daemon.cloud_sync_target = SOSD.daemon.cloud_sync_target_set[0];
    }
    if (SOSD_ECHO_TO_STDOUT) printf("  ... SOSD.daemon.cloud_sync_target == %d\n", SOSD.daemon.cloud_sync_target);
    if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");
    /* -------------------- */


    return 0;
}


int SOSD_cloud_start() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_start");
    int rc;

    if (SOS->role != SOS_ROLE_DB) {
        if (SOSD_ECHO_TO_STDOUT) printf("Launching cloud_sync flush/send thread...\n");
        SOSD.sync.cloud.handler = (pthread_t *) malloc(sizeof(pthread_t));
        rc = pthread_create(SOSD.sync.cloud.handler, NULL, (void *) SOSD_THREAD_cloud_flush, (void *) &SOSD.sync.cloud);
        if (SOSD_ECHO_TO_STDOUT) printf("  ... done.\n");
    }

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
    pthread_join(*SOSD.sync.cloud.handler, NULL);
    free(SOSD.sync.cloud.handler);

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
