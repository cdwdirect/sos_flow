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
#include "pack_buffer.h"

pthread_t *SOSD_cloud_flush;
bool SOSD_cloud_shutdown_underway;

void SOSD_cloud_shutdown_notice(void) {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_shutdown_notice");
    /* NOTE: This function is to facilitate notification of sosd components
     *       that might not be listening to a socket.
     *       Only certain ranks will participate in it.
     */

    dlog(1, "[%s]: Providing shutdown notice to the cloud_sync backend...\n", whoami);
    SOSD_cloud_shutdown_underway = true;

    if (SOS.config.comm_rank < SOSD.daemon.cloud_sync_target_count) {
        dlog(1, "[%s]:   ... preparing notice to SOS_ROLE_DB at rank %d\n", whoami, SOSD.daemon.cloud_sync_target);
        /* The first N ranks will notify the N databases... */
        SOS_msg_header header;
        unsigned char shutdown_msg[128];
        int count;
        int pack_inset;
        int offset;
        count = 1;
        memset(shutdown_msg, '\0', 128);
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
        header.msg_from = SOS.my_guid;
        header.pub_guid = 0;
        pack_inset = SOS_buffer_pack(shutdown_msg, "i", count);
        offset = SOS_buffer_pack((shutdown_msg + pack_inset), "iill",
                                 header.msg_size,
                                 header.msg_type,
                                 header.msg_from,
                                 header.pub_guid  );
        header.msg_size = offset;
        SOS_buffer_pack(shutdown_msg, "ii", count, header.msg_size);

        dlog(1, "[%s]:   ... sending notice\n", whoami);
        MPI_Send((void *) shutdown_msg, header.msg_size, MPI_CHAR, SOSD.daemon.cloud_sync_target, 0, MPI_COMM_WORLD);
        dlog(1, "[%s]:   ... sent successfully\n", whoami);
    }
    
    dlog(1, "[%s]:   ... waiting at barrier for the rest of the sosd daemons\n", whoami);
    MPI_Barrier(MPI_COMM_WORLD);
    dlog(1, "[%s]:   ... done\n", whoami);

    return;
}



void* SOSD_THREAD_cloud_flush(void *params) {
    SOS_SET_WHOAMI(whoami, "SOSD_THREAD_cloud_flush(MPI)");
    SOS_async_buf_pair *bp = (SOS_async_buf_pair *) params;
    struct timespec tsleep;
    struct timeval  tnow;
    int wake_type;

    /* Since the log files are not even open yet, we must wait to use dlog() */
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[%s]: Starting thread.\n", whoami); }
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[%s]:   ... LOCK bp->flush_lock\n", whoami); }
    pthread_mutex_lock(bp->flush_lock);
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[%s]:   ... UNLOCK bp->flush_lock, waiting for daemon to finish initializing.\n", whoami); }
    pthread_cond_wait(bp->flush_cond, bp->flush_lock);
    dlog(6, "[%s]:   ... Woken up!  (LOCK on bp->flush_lock)\n", whoami);

    if (SOS.role == SOS_ROLE_DB) {
        dlog(0, "[%s]: WARNING!  Returning from the SOSD_THREAD_cloud_flush routine, not used by DB.\n", whoami);
        return NULL;
    }

    gettimeofday(&tnow, NULL);
    tsleep.tv_sec  = tnow.tv_sec  + 0;
    tsleep.tv_nsec = (1000 * tnow.tv_usec) + 62500000;   /* ~ 0.06 seconds. */

    dlog(6, "[%s]:   ... entering loop\n", whoami);
    while (SOSD.daemon.running) {
        dlog(6, "[%s]: Sleeping!    (UNLOCK on bp->flush_lock)\n", whoami);
        wake_type = pthread_cond_timedwait(bp->flush_cond, bp->flush_lock, &tsleep);
        dlog(6, "[%s]: Waking up!   (LOCK on bp->flush_lock)\n", whoami);

        if (SOSD_cloud_shutdown_underway) {
            dlog(6, "[%s]:   ... shutdown is imminent, bailing out\n", whoami);
            break;
        }

        dlog(6, "[%s]:   ... bp->grow_buf->entry_count == %d\n", whoami, bp->grow_buf->entry_count);
        dlog(6, "[%s]:   ... bp->send_buf->entry_count == %d\n", whoami, bp->send_buf->entry_count);
        if (wake_type == ETIMEDOUT) {
            dlog(6, "[%s]:   ... timed-out\n", whoami);
        } else {
            dlog(6, "[%s]:   ... manually triggered\n", whoami);
        }

        if (bp->send_buf->entry_count == 0) {
            dlog(6, "[%s]:   ... nothing to do, going back to sleep.\n", whoami);
            gettimeofday(&tnow, NULL);
            tsleep.tv_sec  = tnow.tv_sec  + 0;
            tsleep.tv_nsec = (1000 * tnow.tv_usec) + 122500000UL;   /* ~ 0.12 seconds */
            continue;
        }

        SOSD_cloud_send(bp->send_buf->data, bp->send_buf->len);

        bp->send_buf->len = 0;
        bp->send_buf->entry_count = 0;
        memset(bp->send_buf->data, '\0', bp->send_buf->max);

        /* Done.  Go back to sleep. */
        gettimeofday(&tnow, NULL);
        tsleep.tv_sec  = tnow.tv_sec  + 5;
        tsleep.tv_nsec = tnow.tv_usec + 62500000;   /* ~ 0.06 seconds. */
    }

    dlog(6, "[%s]:   ... UNLOCK bp->flush_lock\n", whoami);
    pthread_mutex_unlock(bp->flush_lock);
    dlog(1, "[%s]:   ... exiting thread safely.\n", whoami);

    return NULL;
}


void SOSD_cloud_enqueue(unsigned char *msg, int msg_len) {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_enqueue");

    if (SOSD_cloud_shutdown_underway) { return; }
    if (msg_len == 0) {
        dlog(1, "[%s]: ERROR: You attempted to enqueue a zero-length message.\n", whoami);
        return;
    }

    SOS_msg_header header;
    memset(&header, '\0', sizeof(SOS_msg_header));

    SOS_buffer_unpack(msg, "iill",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);           

    dlog(6, "[%s]: Enqueueing a %s message of %d bytes...\n", whoami, SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), msg_len);
    if (msg_len != header.msg_size) { dlog(1, "[%s]:   ... ERROR: msg_size(%d) != header.msg_size(%d)", whoami, msg_len, header.msg_size); }
    SOS_async_buf_pair_insert(SOSD.cloud_bp, msg, msg_len);
    dlog(1, "[%s]:   ... done.\n", whoami);

    return;
}


void SOSD_cloud_fflush(void) {
    SOS_async_buf_pair_fflush(SOSD.cloud_bp);
    return;
}


int SOSD_cloud_send(unsigned char *msg, int msg_len) {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_send(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;

    int   entry_count;

    SOS_buffer_unpack(msg, "i", &entry_count);

    dlog(5, "[%s]: -----------> ----> -------------> ----------> ------------->\n", whoami);
    dlog(5, "[%s]: ----> --> >>Transporting off-node!>> ---(%d entries)---->\n", whoami, entry_count);
    dlog(5, "[%s]: ---------------> ---------> --------------> ----> -----> -->\n", whoami);

    /* At this point, it's pretty simple: */
    MPI_Send((void *) msg, msg_len, MPI_CHAR, SOSD.daemon.cloud_sync_target, 0, MPI_COMM_WORLD);

    return 0;
}



void SOSD_cloud_listen_loop(void) {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_listen_loop(MPI)");
    MPI_Status status;
    SOS_async_buf_pair *bp;
    unsigned char *ptr;
    int offset;
    int entry_count, entry;
    SOS_msg_header header;

    SOS_async_buf_pair_init(&bp);  /* Since MPI_ is serial here, we can simply ignore the mutexes. */

    while(SOSD.daemon.running) {
        entry_count = 0;
        offset = 0;
        ptr = (bp->a.data + offset);
        memset(bp->a.data, '\0', bp->a.max); bp->a.len = 0;
        memset(bp->b.data, '\0', bp->b.max); bp->b.len = 0;

        /* Receive a composite message from a daemon: */
        dlog(5, "[%s]: Waiting for a message from MPI...\n", whoami);
        MPI_Recv((void *) bp->a.data, bp->a.max, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        dlog(5, "[%s]:   ... message received!\n", whoami);

        offset += SOS_buffer_unpack(bp->a.data, "i", &entry_count);
        ptr = (bp->a.data + offset);
        dlog(5, "[%s]:   ... message contains %d entries.\n", whoami, entry_count);

        /* Extract one-at-a-time single messages into bp->b.data: */
        for (entry = 1; entry <= entry_count; entry++) {
            dlog(6, "[%s]:   ... processing entry %d of %d @ offset == %d \n", whoami, entry, entry_count, offset);
            memset(&header, '\0', sizeof(SOS_msg_header));
            memset(bp->b.data, '\0', bp->b.max);
            SOS_buffer_unpack(ptr, "iill",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.pub_guid);
            dlog(6, "[%s]:      ... header.msg_size == %d\n", whoami, header.msg_size);
            dlog(6, "[%s]:      ... header.msg_type == %s  (%d)\n", whoami, SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE), header.msg_type);
            dlog(6, "[%s]:      ... header.msg_from == %ld\n", whoami, header.msg_from);
            dlog(6, "[%s]:      ... header.pub_guid == %ld\n", whoami, header.pub_guid);

            memcpy(bp->b.data, ptr, header.msg_size);
            offset += header.msg_size;
            ptr = (bp->a.data + offset);

            /* Now let's handle the message we've moved over into buffer bp-b.data */
            switch (header.msg_type) {

            case SOS_MSG_TYPE_ANNOUNCE:   SOSD_handle_announce   (bp->b.data, header.msg_size); break;
            case SOS_MSG_TYPE_PUBLISH:    SOSD_handle_publish    (bp->b.data, header.msg_size); break;
            case SOS_MSG_TYPE_VAL_SNAPS:  SOSD_handle_val_snaps  (bp->b.data, header.msg_size); break;

            case SOS_MSG_TYPE_SHUTDOWN:   SOSD.daemon.running = 0;

            case SOS_MSG_TYPE_REGISTER:   /* SOSD_handle_register   (bp->b.data, header.msg_size); break; */
            case SOS_MSG_TYPE_GUID_BLOCK: /* SOSD_handle_guid_block (bp->b.data, header.msg_size); break; */
            case SOS_MSG_TYPE_ECHO:       /* SOSD_handle_echo       (bp->b.data, header.msg_size); break; */
            default:                      SOSD_handle_unknown    (bp->b.data, header.msg_size); break;
            }
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

    SOS.config.comm_rank = getpid();
    SOS_SET_WHOAMI(whoami, "SOSD_clout_init");

    SOSD_cloud_shutdown_underway = false;

    if (SOSD_ECHO_TO_STDOUT) printf("[%s]: Configuring this daemon with MPI:\n", whoami);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... calling MPI_Init_thread();\n", whoami);
    rc = MPI_Init_thread( argc, argv, MPI_THREAD_SERIALIZED, &SOS.config.comm_support );
    if (rc != MPI_SUCCESS) {
        MPI_Error_string( rc, mpi_err, &mpi_err_len );
        printf("[%s]:   ... MPI_Init_thread() did not complete successfully!\n", whoami);
        printf("[%s]:   ... Error %d: %s\n", whoami, rc, mpi_err);
        exit( EXIT_FAILURE );
    }
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... safely returned.\n", whoami);

    switch (SOS.config.comm_support) {
    case MPI_THREAD_SINGLE:     if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_SINGLE (could cause problems)\n", whoami); break;
    case MPI_THREAD_FUNNELED:   if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_FUNNELED (could cause problems)\n", whoami); break;
    case MPI_THREAD_SERIALIZED: if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_SERIALIZED\n", whoami); break;
    case MPI_THREAD_MULTIPLE:   if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_MULTIPLE\n", whoami); break;
    default:                    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... WARNING!  The supported threading model (%d) is unrecognized!\n", whoami, SOS.config.comm_support); break;
    }
    rc = MPI_Comm_rank( MPI_COMM_WORLD, &SOS.config.comm_rank );
    rc = MPI_Comm_size( MPI_COMM_WORLD, &SOS.config.comm_size );

    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... rank: %d\n", whoami, SOS.config.comm_rank);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... size: %d\n", whoami, SOS.config.comm_size);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... done.\n", whoami);


    /* ----- Setup the cloud_sync target: ----------*/
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]: Broadcasting role and determining cloud_sync target...\n", whoami);
    sosd_roles = (int *) malloc(SOS.config.comm_size * sizeof(int));
    memset(sosd_roles, '\0', (SOS.config.comm_size * sizeof(int)));
    my_role = (int *) malloc(SOS.config.comm_size * sizeof(int));
    memset(my_role, '\0', (SOS.config.comm_size * sizeof(int)));
    my_role[SOS.config.comm_rank] = SOS.role;
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... sending SOS.role   (%s)\n", whoami, SOS_ENUM_STR(SOS.role, SOS_ROLE));
    MPI_Allreduce((void *) my_role, (void *) sosd_roles, SOS.config.comm_size, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... all roles recieved\n", whoami);
    /* Count the SOS_ROLE_DB's to find out how large our list need be...*/
    cloud_sync_target_count = 0;
    for (this_node = 0; this_node < SOS.config.comm_size; this_node++) {
        if (sosd_roles[this_node] == SOS_ROLE_DB) { cloud_sync_target_count++; }
    }
    if (cloud_sync_target_count == 0) {
        printf("[%s]: ERROR!  No daemon's are set to receive cloud_sync messages!\n", whoami);
        exit(EXIT_FAILURE);
    }
    SOSD.daemon.cloud_sync_target_set = (int *) malloc(cloud_sync_target_count * sizeof(int));
    memset(SOSD.daemon.cloud_sync_target_set, '\0', (cloud_sync_target_count * sizeof(int)));
    /* Compile the list of the SOS_ROLE_DB's ...*/
    cloud_sync_target_count = 0;
    for (this_node = 0; this_node < SOS.config.comm_size; this_node++) {
        if (sosd_roles[this_node] == SOS_ROLE_DB) {
            SOSD.daemon.cloud_sync_target_set[cloud_sync_target_count] = this_node;
            cloud_sync_target_count++;
        }
    }
    SOSD.daemon.cloud_sync_target_count = cloud_sync_target_count;
    memset(my_role, '\0', (SOS.config.comm_size * sizeof(int)));
    memset(sosd_roles, '\0', (SOS.config.comm_size * sizeof(int)));
    free(my_role);
    free(sosd_roles);
    /* Select the SOS_ROLE_DB we're going to cloud_sync with... */
    if (SOS.config.comm_rank > 0) {
        SOSD.daemon.cloud_sync_target =                                 \
            SOSD.daemon.cloud_sync_target_set[SOS.config.comm_rank % SOSD.daemon.cloud_sync_target_count];
    } else {
        SOSD.daemon.cloud_sync_target = SOSD.daemon.cloud_sync_target_set[0];
    }
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... SOSD.daemon.cloud_sync_target == %d\n", whoami, SOSD.daemon.cloud_sync_target);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... done.\n", whoami);
    /* -------------------- */

    if (SOSD_ECHO_TO_STDOUT) printf("[%s]: Initializing cloud_sync buffers...\n", whoami);
    SOS_async_buf_pair_init(&SOSD.cloud_bp);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... done.\n", whoami);

    /* All DAEMON except the DB need the cloud_sync flush thread. */
    if (SOS.role != SOS_ROLE_DB) {
        if (SOSD_ECHO_TO_STDOUT) printf("[%s]: Launching cloud_sync flush/send thread...\n", whoami);
        SOSD_cloud_flush = (pthread_t *) malloc(sizeof(pthread_t));
        rc = pthread_create(SOSD_cloud_flush, NULL, (void *) SOSD_THREAD_cloud_flush, (void *) SOSD.cloud_bp);
        if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... done.\n", whoami);
    }

    return 0;
}

int SOSD_cloud_finalize(void) {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_finalize(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;

    if (SOS.role != SOS_ROLE_DB) {
        dlog(1, "[%s]: Shutting down SOSD cloud services...\n", whoami);
        dlog(1, "[%s]:   ... forcing the cloud_sync buffer to flush.  (flush thread exits)\n", whoami);
        SOS_async_buf_pair_fflush(SOSD.cloud_bp);
        pthread_cond_signal(SOSD.cloud_bp->flush_cond);
        dlog(1, "[%s]:   ... joining the cloud_sync flush thread.\n", whoami);
        pthread_join(*SOSD_cloud_flush, NULL);
        free(SOSD_cloud_flush);
    }

    dlog(1, "[%s]:   ... cleaning up the cloud_sync_set list.\n", whoami);
    memset(SOSD.daemon.cloud_sync_target_set, '\0', (SOSD.daemon.cloud_sync_target_count * sizeof(int)));
    free(SOSD.daemon.cloud_sync_target_set);
    dlog(1, "[%s]:   ... destroying the cloud-send buffers.\n", whoami);
    SOS_async_buf_pair_destroy(SOSD.cloud_bp);

    dlog(1, "[%s]: Leaving the MPI communicator...\n", whoami);
    rc = MPI_Finalize();
    if (rc != MPI_SUCCESS) {
        MPI_Error_string( rc, mpi_err, &mpi_err_len );
        dlog(1, "[%s]:   ... MPI_Finalize() did not complete successfully!\n", whoami);
        dlog(1, "[%s]:   ... Error %d: %s\n", whoami, rc, mpi_err);
    }
    dlog(1, "[%s]:   ... Clearing the SOS.config.comm_* fields.\n", whoami);
    SOS.config.comm_rank    = -1;
    SOS.config.comm_size    = -1;
    SOS.config.comm_support = -1;
    dlog(1, "[%s]:   ... done.\n", whoami);

    return 0;
}
