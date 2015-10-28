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


SOS_async_buf_pair *SOSD_cloud_bp;
pthread_t *SOSD_cloud_flush;


void* SOSD_THREAD_cloud_flush(void *params) {
    SOS_SET_WHOAMI(whoami, "SOSD_THREAD_cloud_flush(MPI)");
    SOS_async_buf_pair *bp = (SOS_async_buf_pair *) params;
    struct timespec ts;
    struct timeval  tp;
    int wake_type;

    gettimeofday(&tp, NULL); ts.tv_nsec += (tp.tv_usec * 500000);
    pthread_mutex_lock(bp->send_buf->lock);
    while (SOSD.daemon.running) {
        wake_type = pthread_cond_timedwait(bp->flush_cond, bp->send_buf->lock, &ts);
        if (wake_type == ETIMEDOUT) {
            /* ...any special actions if timed-out vs. manual trigger */
            if (bp->send_buf->entry_count == 0) {
                /* if there is nothing to do, sleep a bit longer, assuming activity is bursty. */
                gettimeofday(&tp, NULL); ts.tv_sec += 1;
                continue;
            }
        }

        SOSD_cloud_send(bp->send_buf->data, bp->send_buf->len);

        /* Done.  Go back to sleep. */
        gettimeofday(&tp, NULL); ts.tv_nsec += (tp.tv_usec * 500000);
    }

    return NULL;
}

int SOSD_cloud_send(char *msg, int msg_len) {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_send(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;

    /* At this point, it's pretty simple: */
    MPI_Send((void *) msg, msg_len, MPI_CHAR, SOSD.daemon.cloud_sync_target, 0, MPI_COMM_WORLD);

    return 0;
}



void SOSD_cloud_listen_loop(void) {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_listen_loop(MPI)");
    MPI_Status status;
    SOS_async_buf_pair *bp;
    char *ptr;
    int offset;
    int elem_count, elem;
    SOS_msg_header header;

    SOS_async_buf_pair_init(&bp);  /* Since MPI_ is serial here, we can simply ignore the mutexes. */

    /* TODO: SOS_ROLE_DB { Who is responsible for terminating DB's?  They don't use a socket. } */
    while(SOSD.daemon.running) {
        elem_count = 0;
        offset = 0;
        ptr = (bp->a.data + offset);
        memset(bp->a.data, '\0', bp->a.max); bp->a.len = 0;
        memset(bp->b.data, '\0', bp->b.max); bp->b.len = 0;

        /* Receive a composite message from a daemon: */
        MPI_Recv((void *) bp->a.data, bp->a.max, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        offset += SOS_buffer_unpack(bp->a.data, "i", &elem_count);
        ptr = (bp->a.data + offset);

        /* Extract one-at-a-time single messages into bp->b.data: */
        for (elem = 0; elem < elem_count; elem++) {
            memset(&header, '\0', sizeof(SOS_msg_header));
            SOS_buffer_unpack(ptr, "iill",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.pub_guid);
            memcpy(bp->b.data, ptr, header.msg_size);
            offset += header.msg_size;
            ptr = (bp->a.data + offset);

            /* Now let's handle the message we've moved over into buffer bp-b.data */
            switch (header.msg_type) {
            case SOS_MSG_TYPE_REGISTER:   SOSD_handle_register   (bp->b.data, header.msg_size); break; 
            case SOS_MSG_TYPE_GUID_BLOCK: SOSD_handle_guid_block (bp->b.data, header.msg_size); break;
            case SOS_MSG_TYPE_ANNOUNCE:   SOSD_handle_announce   (bp->b.data, header.msg_size); break;
            case SOS_MSG_TYPE_PUBLISH:    SOSD_handle_publish    (bp->b.data, header.msg_size); break;
            case SOS_MSG_TYPE_ECHO:       SOSD_handle_echo       (bp->b.data, header.msg_size); break;
            case SOS_MSG_TYPE_SHUTDOWN:   SOSD_handle_shutdown   (bp->b.data, header.msg_size); break;
            default:                      SOSD_handle_unknown    (bp->b.data, header.msg_size); break;
            }

            /* TODO: {SOSD_DB} This is not the most efficient thing in the world, but good
             *       enough for early demo work.  Ultimately, the DB should have a more
             *       streamlined memory structure than a full-fledged DAEMON. */
            
        }

    }

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
    SOS_async_buf_pair_init(&SOSD_cloud_bp);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... done.\n", whoami);

    if (SOSD_ECHO_TO_STDOUT) printf("[%s]: Launching cloud_sync flush/send thread...\n", whoami);
    SOSD_cloud_flush = (pthread_t *) malloc(sizeof(pthread_t));
    rc = pthread_create(SOSD_cloud_flush, NULL, (void *) SOSD_THREAD_cloud_flush, (void *) SOSD_cloud_bp);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... done.\n", whoami);

    return 0;
}

int SOSD_cloud_finalize() {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_finalize(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;

    dlog(1, "[%s]: Shutting down SOSD cloud services...\n", whoami);
    dlog(1, "[%s]:   ... forcing the cloud_sync buffer to flush.  (flush thread exits)\n", whoami);
    SOS_async_buf_pair_fflush(SOSD_cloud_bp);
    dlog(1, "[%s]:   ... joining the cloud_sync flush thread.\n", whoami);
    pthread_join(*SOSD_cloud_flush, NULL);
    free(SOSD_cloud_flush);
    dlog(1, "[%s]:   ... cleaning up the cloud_sync_set list.\n", whoami);
    memset(SOSD.daemon.cloud_sync_target_set, '\0', (SOSD.daemon.cloud_sync_target_count * sizeof(int)));
    free(SOSD.daemon.cloud_sync_target_set);
    dlog(1, "[%s]:   ... destroying the cloud-send buffers.\n", whoami);
    SOS_async_buf_pair_destroy(SOSD_cloud_bp);

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
