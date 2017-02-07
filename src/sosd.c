/*
 *  sosd.c (daemon)
 *
 *
 *
 */

#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
    #define OPT_PARAMS "\n" \
                       "optional parameter (EVPath only):\n" \
                       "\n" \
                       "                 -m, --master dfg\n" \
                       "\n" \
                       "                                   Coordinate the EVPath initialization.\n" \
                       "                         Only one sosd instance is given this parameter.\n"
#else
    #define OPT_PARAMS " "
#endif


#define USAGE          "usage:   $ sosd  -l, --listeners <count>\n" \
                       "                 -a, --aggregators <count>\n" \
                       "                 -w, --work_dir <full_path>\n" \
                       OPT_PARAMS


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef SOSD_CLOUD_SYNC_WITH_MPI
#include "sosd_cloud_mpi.h"
#endif
#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
#include "sosd_cloud_evpath.h"
#endif
#ifdef SOSD_CLOUD_SYNC_WITH_STUBS
#include "sosd_cloud_stubs.h"
#endif

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_db_sqlite.h"

#include "sos_pipe.h"
#include "sos_qhashtbl.h"
#include "sos_buffer.h"


void SOSD_display_logo(void);

int main(int argc, char *argv[])  {
    int elem, next_elem;
    int retval;
    SOS_role my_role;

    /* [countof]
     *    statistics for daemon activity.
     */
    memset(&SOSD.daemon.countof, 0, sizeof(SOSD_counts));
    if (SOS_DEBUG > 0) {
        SOSD.daemon.countof.lock_stats = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(SOSD.daemon.countof.lock_stats, NULL);
    }

    SOSD.daemon.work_dir    = (char *) &SOSD_DEFAULT_DIR;
    SOSD.daemon.name        = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
    SOSD.daemon.lock_file   = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
    SOSD.daemon.log_file    = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);

    my_role = SOS_ROLE_UNASSIGNED;

    SOSD.net.listen_backlog = 10;


    // Grab the port from the environment variable SOS_CMD_PORT
    SOSD.net.server_port = getenv("SOS_CMD_PORT");
    SOSD.net.port_number = atoi(SOSD.net.server_port);

    /* Process command-line arguments */
    if ( argc < 7 ) { fprintf(stderr, "ERROR: Invalid number of arguments supplied.   (%d)\n\n%s\n", argc, USAGE); exit(EXIT_FAILURE); }
    SOSD.net.listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) { fprintf(stderr, "ERROR: Incorrect parameter pairing.\n\n%s\n", USAGE); exit(EXIT_FAILURE); }
        if (      (strcmp(argv[elem], "--listeners"       ) == 0)
        ||        (strcmp(argv[elem], "-l"                ) == 0)) {
            SOSD.daemon.listener_count = atoi(argv[next_elem]);
        }
        else if ( (strcmp(argv[elem], "--aggregators"     ) == 0)
        ||        (strcmp(argv[elem], "-a"                ) == 0)) {
            SOSD.daemon.aggregator_count = atoi(argv[next_elem]);
        }
        else if ( (strcmp(argv[elem], "--work_dir"        ) == 0)
        ||        (strcmp(argv[elem], "-w"                ) == 0)) {
            SOSD.daemon.work_dir    = argv[next_elem];
        }
        else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    if ((SOSD.net.port_number < 1) && (my_role == SOS_ROLE_UNASSIGNED)) {
        fprintf(stderr, "ERROR: No port was specified for the daemon to monitor.\n\n%s\n", USAGE); exit(EXIT_FAILURE);
    }


    #ifndef SOSD_CLOUD_SYNC
    if (my_role != SOS_ROLE_LISTENER) {
        printf("NOTE: Terminating an instance of sosd with pid: %d\n", getpid());
        printf("NOTE: SOSD_CLOUD_SYNC is disabled but this instance is not a SOS_ROLE_LISTENER!\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    #endif


    unsetenv("SOS_SHUTDOWN");

    memset(&SOSD.daemon.pid_str, '\0', 256);

    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("Preparing to initialize:\n"); fflush(stdout); }
    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("   ... creating SOS_runtime object for daemon use.\n"); fflush(stdout); }
    SOSD.sos_context = (SOS_runtime *) malloc(sizeof(SOS_runtime));
    memset(SOSD.sos_context, '\0', sizeof(SOS_runtime));
    SOSD.sos_context->role = SOS_ROLE_UNASSIGNED;

    #ifdef SOSD_CLOUD_SYNC
    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("   ... calling SOSD_cloud_init()...\n"); fflush(stdout); }
    SOSD_cloud_init( &argc, &argv);
    #else
    dlog(0, "   ... WARNING: There is no CLOUD_SYNC configured for this SOSD.\n");
    #endif

    SOS_SET_CONTEXT(SOSD.sos_context, "main");
    my_role = SOS->role;

    dlog(0, "Initializing SOSD:\n");
    dlog(0, "   ... calling SOS_init(argc, argv, %s, SOSD.sos_context) ...\n", SOS_ENUM_STR( SOS->role, SOS_ROLE ));
    SOSD.sos_context = SOS_init_with_runtime( &argc, &argv, my_role, SOS_LAYER_SOS_RUNTIME, SOSD.sos_context );

    dlog(0, "   ... calling SOSD_init()...\n");
    SOSD_init();
    if (SOS->config.comm_rank == 0) {
        SOSD_display_logo();
    }

    dlog(0, "   ... done. (SOSD_init + SOS_init are complete)\n");
    dlog(0, "Calling register_signal_handler()...\n");
    if (SOSD_DAEMON_LOG) SOS_register_signal_handler(SOSD.sos_context);

    //GO
    //TODO: Add support for socket interactions to the AGGREGATOR roles...
    if (SOS->role == SOS_ROLE_LISTENER) {
        dlog(0, "Calling daemon_setup_socket()...\n");
        SOSD_setup_socket();
    }

    dlog(0, "Calling daemon_init_database()...\n");
    SOSD_db_init_database();

    dlog(0, "Initializing the sync framework...\n");
    SOSD_sync_context_init(SOS, &SOSD.sync.db,   sizeof(SOSD_db_task *), SOSD_THREAD_db_sync);
    #ifdef SOSD_CLOUD_SYNC
    if (SOS->role == SOS_ROLE_LISTENER) {
        SOSD_sync_context_init(SOS, &SOSD.sync.cloud, sizeof(SOS_buffer *), SOSD_THREAD_cloud_sync);
        SOSD_cloud_start();
    }
    #else
    #endif
    SOSD_sync_context_init(SOS, &SOSD.sync.local, sizeof(SOS_buffer *), SOSD_THREAD_local_sync);


    dlog(0, "Entering listening loop...\n");

    /* Go! */
    switch (SOS->role) {
    case SOS_ROLE_LISTENER:
        SOS->config.locale = SOS_LOCALE_APPLICATION;
        SOSD_listen_loop();
        break;

    case SOS_ROLE_AGGREGATOR:
        SOS->config.locale = SOS_LOCALE_DAEMON_DBMS;
        #ifdef SOSD_CLOUD_SYNC
        SOSD_cloud_listen_loop();
        #endif
        break;
    default: break;
    }

    /* Done!  Cleanup and shut down. */


    dlog(0, "Closing the sync queues:\n");
    if (SOSD.sync.local.queue != NULL) {
        dlog(0, "  .. SOSD.sync.local.queue\n");
        pipe_producer_free(SOSD.sync.local.queue->intake);
    }
    if (SOSD.sync.cloud.queue != NULL) {
        dlog(0, "  .. SOSD.sync.cloud.queue\n");
        pipe_producer_free(SOSD.sync.cloud.queue->intake);
    }
    if (SOSD.sync.db.queue != NULL) {
        dlog(0, "  .. SOSD.sync.db.queue\n");
        pipe_producer_free(SOSD.sync.db.queue->intake);
    }

    /* TODO: { SHUTDOWN } Add cascading queue fflush here to prevent deadlocks. */
    //dlog(0, "     (waiting for local.queue->elem_count == 0)\n");
    //dlog(0, "  .. SOSD.sync.cloud.queue\n");
    //pipe_producer_free(SOSD.sync.cloud.queue->intake);
    //dlog(0, "  .. SOSD.sync.db.queue\n");
    //pipe_producer_free(SOSD.sync.db.queue->intake);


    SOS->status = SOS_STATUS_HALTING;
    SOSD.db.ready = -1;

    dlog(0, "Destroying uid configurations.\n");
    SOS_uid_destroy( SOSD.guid );
    dlog(0, "  ... done.\n");
    dlog(0, "Closing the database.\n");
    SOSD_db_close_database();
    if (SOS->role == SOS_ROLE_LISTENER) {
        dlog(0, "Closing the socket.\n");
        shutdown(SOSD.net.server_socket_fd, SHUT_RDWR);
    }
    #if (SOSD_CLOUD_SYNC > 0)
    dlog(0, "Detaching from the cloud of sosd daemons.\n");
    SOSD_cloud_finalize();
    #endif

    dlog(0, "Shutting down SOS services.\n");
    SOS_finalize(SOS);

    if (SOSD_DAEMON_LOG) { fclose(sos_daemon_log_fptr); }
    if (SOSD_DAEMON_LOG) { free(SOSD.daemon.log_file); }
    if (SOS_DEBUG > 0)   {
        pthread_mutex_lock(SOSD.daemon.countof.lock_stats);
        pthread_mutex_destroy(SOSD.daemon.countof.lock_stats);
        free(SOSD.daemon.countof.lock_stats);
    }

    close(sos_daemon_lock_fptr);
    remove(SOSD.daemon.lock_file);
    free(SOSD.daemon.name);
    free(SOSD.daemon.lock_file);

    return(EXIT_SUCCESS);
} //end: main()




void SOSD_listen_loop() {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_listen_loop");
    SOS_msg_header header;
    SOS_buffer    *buffer;
    SOS_buffer    *rapid_reply;
    int            recv_len;
    int            offset;
    int            i;

    SOS_buffer_init_sized_locking(SOS, &buffer, SOS_DEFAULT_BUFFER_MAX, false);
    SOS_buffer_init_sized_locking(SOS, &rapid_reply, SOS_DEFAULT_REPLY_LEN, false);

    SOSD_PACK_ACK(rapid_reply);

    dlog(0, "Entering main loop...\n");
    while (SOSD.daemon.running) {
        offset = 0;
        SOS_buffer_wipe(buffer);

        dlog(5, "Listening for a message...\n");
        SOSD.net.peer_addr_len = sizeof(SOSD.net.peer_addr);
        SOSD.net.client_socket_fd = accept(SOSD.net.server_socket_fd, (struct sockaddr *) &SOSD.net.peer_addr, &SOSD.net.peer_addr_len);
        i = getnameinfo((struct sockaddr *) &SOSD.net.peer_addr, SOSD.net.peer_addr_len, SOSD.net.client_host, NI_MAXHOST, SOSD.net.client_port, NI_MAXSERV, NI_NUMERICSERV);
        if (i != 0) { dlog(0, "Error calling getnameinfo() on client connection.  (%s)\n", strerror(errno)); break; }

        buffer->len = recv(SOSD.net.client_socket_fd, (void *) buffer->data, buffer->max, 0);
        dlog(6, "  ... recv() returned %d bytes.\n", buffer->len);

        if (buffer->len < 0) {
            dlog(1, "  ... recv() call returned an errror.  (%s)\n", strerror(errno));
        }

        memset(&header, '\0', sizeof(SOS_msg_header));
        if (buffer->len >= sizeof(SOS_msg_header)) {
            int offset = 0;
            SOS_buffer_unpack(buffer, &offset, "iigg",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.pub_guid);
        } else {
            dlog(0, "  ... Received short (useless) message.\n");
            continue;
        }

        /* Check the size of the message. We may not have gotten it all. */
        while (header.msg_size > buffer->len) {
            int old = buffer->len;
            while (header.msg_size > buffer->max) {
                SOS_buffer_grow(buffer, 1 + (header.msg_size - buffer->max), SOS_WHOAMI);
            }
            int rest = recv(SOSD.net.client_socket_fd, (void *) (buffer->data + old), header.msg_size - old, 0);
            if (rest < 0) {
                dlog(1, "  ... recv() call returned an errror.  (%s)\n", strerror(errno));
            } else {
                dlog(6, "  ... recv() returned %d more bytes.\n", rest);
            }
            buffer->len += rest;
        }

        dlog(5, "Received connection.\n");
        dlog(5, "  ... msg_size == %d         (buffer->len == %d)\n", header.msg_size, buffer->len);
        dlog(5, "  ... msg_type == %s\n", SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE));
        if ((header.msg_size != buffer->len) || (header.msg_size > buffer->max)) {
            dlog(0, "ERROR:  BUFFER not correctly sized!  header.msg_size == %d, buffer->len == %d / %d\n",
                 header.msg_size, buffer->len, buffer->max);
        }

        dlog(5, "  ... msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
        dlog(5, "  ... pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);

        SOSD_countof(socket_messages++);
        SOSD_countof(socket_bytes_recv += header.msg_size);

        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER:   SOSD_handle_register   (buffer); break; 
        case SOS_MSG_TYPE_GUID_BLOCK: SOSD_handle_guid_block (buffer); break;

        case SOS_MSG_TYPE_ANNOUNCE:
        case SOS_MSG_TYPE_PUBLISH:
        case SOS_MSG_TYPE_VAL_SNAPS:
            dlog(5, "  ... [ddd] <---- pushing buffer @ [%ld] onto the local_sync queue. buffer->len == %d   (%s)\n", (long) buffer, buffer->len, SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE));
            pthread_mutex_lock(SOSD.sync.local.queue->sync_lock);
            pipe_push(SOSD.sync.local.queue->intake, (void *) &buffer, 1);
            SOSD.sync.local.queue->elem_count++;
            pthread_mutex_unlock(SOSD.sync.local.queue->sync_lock);
            buffer = NULL;
            SOS_buffer_init_sized_locking(SOS, &buffer, SOS_DEFAULT_BUFFER_MAX, false);
            //Send generic ACK message back to the client:
            dlog(5, "  ... sending ACK w/reply->len == %d\n", rapid_reply->len);
            i = send( SOSD.net.client_socket_fd, (void *) rapid_reply->data, rapid_reply->len, 0);
            if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
            else {
                dlog(5, "  ... send() returned the following bytecount: %d\n", i);
                SOSD_countof(socket_bytes_sent += i);
            }    
            dlog(5, "  ... Done.\n");
            break;

        case SOS_MSG_TYPE_ECHO:       SOSD_handle_echo       (buffer); break;
        case SOS_MSG_TYPE_SHUTDOWN:   SOSD_handle_shutdown   (buffer); break;
        case SOS_MSG_TYPE_CHECK_IN:   SOSD_handle_check_in   (buffer); break;
        case SOS_MSG_TYPE_PROBE:      SOSD_handle_probe      (buffer); break;
        case SOS_MSG_TYPE_QUERY:      SOSD_handle_sosa_query (buffer); break;
        default:                      SOSD_handle_unknown    (buffer); break;
        }

        close( SOSD.net.client_socket_fd );
    }

    SOS_buffer_destroy(buffer);
    SOS_buffer_destroy(rapid_reply);

    dlog(1, "Leaving the socket listening loop.\n");

    return;
}

/* -------------------------------------------------- */


void* SOSD_THREAD_local_sync(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_local_sync");
    struct timeval   now;
    struct timespec  wait;
    SOS_msg_header   header;
    SOS_buffer      *buffer;
    int              offset;
    int              count;

    pthread_mutex_lock(my->lock);

    gettimeofday(&now, NULL);
    wait.tv_sec  = SOSD_LOCAL_SYNC_WAIT_SEC  + (now.tv_sec);
    wait.tv_nsec = SOSD_LOCAL_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_cond_timedwait(my->cond, my->lock, &wait);
        buffer = NULL;

        SOSD_countof(thread_local_wakeup++);

        count = pipe_pop_eager(my->queue->outlet, (void *) &buffer, 1);
        if (count == 0) {
            dlog(6, "Nothing remains in the queue, and the intake is closed.  Leaving thread.\n");
            break;
        } else {
            pthread_mutex_lock(my->queue->sync_lock);
            my->queue->elem_count -= count;
            pthread_mutex_unlock(my->queue->sync_lock);
        }

        dlog(6, "  [ddd] >>>>> Popped a buffer @ [%ld] off the queue. ... buffer->len == %d   [&my == %ld]\n", (long) buffer, buffer->len, (long) my);


        if (buffer == NULL) {
            dlog(6, "   ... *buffer == NULL!\n");
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_LOCAL_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_LOCAL_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        int offset = 0;
        SOS_buffer_unpack(buffer, &offset, "iigg",
            &header.msg_size,
            &header.msg_type,
            &header.msg_from,
            &header.pub_guid);
        
        switch(header.msg_type) {
        case SOS_MSG_TYPE_ANNOUNCE:   SOSD_handle_announce   (buffer); break;
        case SOS_MSG_TYPE_PUBLISH:    SOSD_handle_publish    (buffer); break;
        case SOS_MSG_TYPE_VAL_SNAPS:  SOSD_handle_val_snaps  (buffer); break;
        case SOS_MSG_TYPE_KMEAN_DATA: SOSD_handle_kmean_data (buffer); break;
        default:
            dlog(0, "ERROR: An invalid message type (%d) was placed in the local_sync queue!\n", header.msg_type);
            dlog(0, "ERROR: Destroying it.\n");
            SOS_buffer_destroy(buffer);
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_LOCAL_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_LOCAL_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        if (SOS->role == SOS_ROLE_LISTENER) {
            pthread_mutex_lock(SOSD.sync.cloud.queue->sync_lock);
            pipe_push(SOSD.sync.cloud.queue->intake, (void *) &buffer, 1);
            SOSD.sync.cloud.queue->elem_count++;
            pthread_mutex_unlock(SOSD.sync.cloud.queue->sync_lock);
        } else if (SOS->role == SOS_ROLE_AGGREGATOR) {
            //DB role's can go ahead and release the buffer.
            SOS_buffer_destroy(buffer);
        }

        gettimeofday(&now, NULL);
        wait.tv_sec  = SOSD_LOCAL_SYNC_WAIT_SEC  + (now.tv_sec);
        wait.tv_nsec = SOSD_LOCAL_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
    }

    pthread_mutex_unlock(my->lock);
    pthread_exit(NULL);
}


void* SOSD_THREAD_db_sync(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_db_sync");
    struct timeval   now;
    struct timespec  wait;
    SOSD_db_task   **task_list;
    SOSD_db_task    *task;
    int              task_index;
    int              queue_depth;
    int              count;

    pthread_mutex_lock(my->lock);
    gettimeofday(&now, NULL);
    wait.tv_sec  = SOSD_DB_SYNC_WAIT_SEC  + (now.tv_sec);
    wait.tv_nsec = SOSD_DB_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_cond_timedwait(my->cond, my->lock, &wait);

        SOSD_countof(thread_db_wakeup++);

        pthread_mutex_lock(my->queue->sync_lock);

        //queue_depth = SOS_min(SOS_DEFAULT_DB_TRANS_SIZE, my->queue->elem_count);
        queue_depth = my->queue->elem_count;
        if (queue_depth > 0) {
            task_list = (SOSD_db_task **) malloc(queue_depth * sizeof(SOSD_db_task *));
        } else {
            pthread_mutex_unlock(my->queue->sync_lock);
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_DB_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_DB_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        count = 0;
        count = pipe_pop_eager(my->queue->outlet, (void *) task_list, queue_depth);
        if (count == 0) {
            dlog(0, "Nothing remains in the queue and the intake is closed.  Leaving thread.\n");
            free(task_list);
            break;
        }

        my->queue->elem_count -= count;
        pthread_mutex_unlock(my->queue->sync_lock);

        dlog(6, "Popped %d elements into %d spaces.\n", count, queue_depth);


        SOSD_db_transaction_begin();

        for (task_index = 0; task_index < count; task_index++) {
            task = task_list[task_index];
            switch(task->type) {
            case SOS_MSG_TYPE_ANNOUNCE:
                dlog(6, "Sending ANNOUNCE to the database...\n");
                SOSD_db_insert_pub(task->pub);
                break;

            case SOS_MSG_TYPE_PUBLISH:
                dlog(6, "Sending PUBLISH to the database...\n");
                SOSD_db_insert_data(task->pub);
                break;

            case SOS_MSG_TYPE_VAL_SNAPS:
                dlog(6, "Sending VAL_SNAPS to the database...\n");
                SOSD_db_insert_vals(SOSD.db.snap_queue, NULL);
                break;

            default:
                dlog(0, "WARNING: Invalid task->type value at task_list[%d].   (%d)\n",
                     task_index, task->type);
            }
            free(task);
        }

        SOSD_countof(db_transactions++);

        SOSD_db_transaction_commit();

        free(task_list);
        gettimeofday(&now, NULL);
        wait.tv_sec  = SOSD_DB_SYNC_WAIT_SEC  + (now.tv_sec);
        wait.tv_nsec = SOSD_DB_SYNC_WAIT_NSEC + (1000 * now.tv_usec);

    }

    pthread_mutex_unlock(my->lock);
    pthread_exit(NULL);
}



void* SOSD_THREAD_cloud_sync(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_cloud_sync");
    struct timeval   now;
    struct timespec  wait;
    SOS_buffer      *buffer;
    SOS_buffer      *reply;
    SOS_buffer     **msg_list;
    SOS_buffer      *msg;
    SOS_msg_header   header;
    int              msg_index;
    int              queue_depth;
    int              count;
    int              offset;
    int              msg_offset;

    SOS_buffer_init_sized_locking(SOS, &buffer, (1000 * SOS_DEFAULT_BUFFER_MAX), false);
    SOS_buffer_init_sized_locking(SOS, &reply,  (SOS_DEFAULT_BUFFER_MAX),        false);

    pthread_mutex_lock(my->lock);
    gettimeofday(&now, NULL);
    wait.tv_sec  = SOSD_CLOUD_SYNC_WAIT_SEC  + (now.tv_sec);
    wait.tv_nsec = SOSD_CLOUD_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_cond_timedwait(my->cond, my->lock, &wait);

        //dlog(0, "Waking up.\n");
        SOSD_countof(thread_cloud_wakeup++);

        pthread_mutex_lock(my->queue->sync_lock);
        queue_depth = my->queue->elem_count;

        //dlog(0, "   ...queue_depth == %d\n", queue_depth);

        if (queue_depth > 0) {
            msg_list = (SOS_buffer **) malloc(queue_depth * sizeof(SOS_buffer *));
        } else {
            //Going back to sleep.
            pthread_mutex_unlock(my->queue->sync_lock);
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_CLOUD_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_CLOUD_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        count = 0;
        count = pipe_pop_eager(my->queue->outlet, (void *) msg_list, queue_depth);
        if (count == 0) {
            dlog(0, "Nothing in the queue and the intake is closed.  Leaving thread.\n");
            free(msg_list);
            break;
        }

        my->queue->elem_count -= count;
        pthread_mutex_unlock(my->queue->sync_lock);

        offset = 0;
        SOS_buffer_pack(buffer, &offset, "i", count);
        for (msg_index = 0; msg_index < count; msg_index++) {
 
            msg_offset = 0;
            msg = msg_list[msg_index];

            SOS_buffer_unpack(msg, &msg_offset, "iigg",
                &header.msg_size,
                &header.msg_type,
                &header.msg_from,
                &header.pub_guid);

            while ((header.msg_size + offset) > buffer->max) {
                dlog(1, "(header.msg_size == %d   +   offset == %d)  >  buffer->max == %d    (growing...)\n",
                     header.msg_size, offset, buffer->max);
                SOS_buffer_grow(buffer, buffer->max, SOS_WHOAMI);
            }

            dlog(1, "[ccc] (%d of %d) --> packing in msg(%15s).size == %d @ offset:%d\n",
                (msg_index + 1), count, SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE),
                header.msg_size, offset);

            memcpy((buffer->data + offset), msg->data, header.msg_size);
            offset += header.msg_size;

            SOS_buffer_destroy(msg);
        }

        buffer->len = offset;
        dlog(0, "[ccc] Sending %d messages in %d bytes over MPI...\n", count, buffer->len);

        SOSD_cloud_send(buffer, reply);

        /* TODO: { CLOUD, REPLY} Handle any replies here. */

        SOS_buffer_wipe(buffer);
        SOS_buffer_wipe(reply);
        free(msg_list);

        gettimeofday(&now, NULL);
        wait.tv_sec  = SOSD_CLOUD_SYNC_WAIT_SEC  + (now.tv_sec);
        wait.tv_nsec = SOSD_CLOUD_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
    }
    SOS_buffer_destroy(buffer);

    pthread_mutex_unlock(my->lock);
    pthread_exit(NULL);
}


/* -------------------------------------------------- */




void SOSD_handle_kmean_data(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_kmean_data");
    // For parsing the buffer:
    char             pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    SOS_pub         *pub;
    SOS_msg_header   header;
    int              offset;
    int              rc;
    // For unpacking / using the contents:
    SOS_guid         guid;
    SOSD_km2d_point  point;
        
    dlog(5, "header.msg_type = SOS_MSG_TYPE_KMEAN_DATA\n");

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    if (pub == NULL) {
        dlog(0, "ERROR: No pub exists for header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid); 
        dlog(0, "ERROR: Destroying message and returning.\n");
        SOS_buffer_destroy(buffer);
        return;
    }

    //...

    dlog(5, "Nothing to do... returning...");

    return;
}



void SOSD_handle_sosa_query(SOS_buffer *buffer) { 
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_query");
    SOS_msg_header header;
    int            offset;
    int            rc;
    SOS_buffer    *result;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_QUERY\n");

    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    SOS_buffer_init_sized_locking(SOS, &result, SOS_DEFAULT_BUFFER_MAX, false);

    SOSD_db_handle_sosa_query(buffer, result);

    rc = send(SOSD.net.client_socket_fd, (void *) result->data, result->len, 0);
    if (rc == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        SOSD_countof(socket_bytes_sent += rc);
    }
       
    return;
}



void SOSD_handle_echo(SOS_buffer *buffer) { 
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_echo");
    SOS_msg_header header;
    int            offset;
    int            rc;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_ECHO\n");

    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    rc = send(SOSD.net.client_socket_fd, (void *) buffer->data, buffer->len, 0);
    if (rc == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        SOSD_countof(socket_bytes_sent += rc);
    }
       
    return;
}


void SOSD_handle_val_snaps(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_val_snaps");
    SOSD_db_task  *task;
    SOS_msg_header header;
    SOS_pub       *pub;
    char           pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int            offset;
    int            rc;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_VAL_SNAPS\n");

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    if (pub == NULL) {
        dlog(0, "ERROR: No pub exists for header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid); 
        dlog(0, "ERROR: Destroying message and returning.\n");
        SOS_buffer_destroy(buffer);
        return;
    }

    dlog(5, "Injecting snaps into SOSD.db.snap_queue...\n");
    SOS_val_snap_queue_from_buffer(buffer, SOSD.db.snap_queue, pub);


    dlog(5, "Queue these val snaps up for the database...\n");
    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->pub = pub;
    task->type = SOS_MSG_TYPE_VAL_SNAPS;

    //pthread_mutex_lock(SOSD.db.snap_queue->sync_lock);
    //if (SOSD.db.snap_queue->sync_pending == 0) {
    //    SOSD.db.snap_queue->sync_pending = 1;
    //    pthread_mutex_unlock(SOSD.db.snap_queue->sync_lock);
        pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
        pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
        SOSD.sync.db.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);
    //} else {
    //    pthread_mutex_unlock(SOSD.db.snap_queue->sync_lock);
    //}

    dlog(5, "  ... done.\n");


    // Spin off the k-means information to be treated
    //   in parallel with its database injection:
    if ((pub->meta.nature == SOS_NATURE_KMEAN_2D)
        && (SOS->role == SOS_ROLE_LISTENER)) {
        dlog(4, "Re-queing the k-means task for processing...\n");
        SOS_buffer *copy;
        // Make a copy of this buffer.
        SOS_buffer_clone(&copy, buffer); 
        // Set it to be the KMEAN type of data:
        offset = 0;
        header.msg_type = SOS_MSG_TYPE_KMEAN_DATA;
        SOS_buffer_pack(copy, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);
        // Enqueue it to be handled:
        pthread_mutex_lock(SOSD.sync.local.queue->sync_lock);
        pipe_push(SOSD.sync.local.queue->intake, (void *)&copy, 1);
        SOSD.sync.local.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.local.queue->sync_lock);
    }



    return;
}



/* TODO: { VERSIONING } Add SOSD version to this message. */
void SOSD_handle_register(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_register");
    SOS_msg_header header;
    SOS_buffer    *reply;
    int            offset;
    int            i;
    SOS_guid       guid_block_from;
    SOS_guid       guid_block_to;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_REGISTER\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);


    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);

    if (header.msg_from == 0) {
        /* A new client is registering with the daemon.
         * Supply them a block of GUIDs ...
         */
        SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &guid_block_from, &guid_block_to);

        offset = 0;
        SOS_buffer_pack(reply, &offset, "gg",
                        guid_block_from,
                        guid_block_to);

    } else {
        /* An existing client (such as a scripting language wrapped library)
         * is coming back online, so don't give them any GUIDs.
         */
        SOSD_PACK_ACK(reply);
    }

    i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
        SOSD_countof(socket_bytes_sent += i);
    }

    SOS_buffer_destroy(reply);

    return;
}


void SOSD_handle_guid_block(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_guid_block");
    SOS_msg_header header;
    SOS_guid       block_from   = 0;
    SOS_guid       block_to     = 0;
    SOS_buffer    *reply;
    int            offset;
    int            i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_GUID_BLOCK\n");

    SOS_buffer_init_sized_locking(SOS, &reply, (2 * sizeof(uint64_t)), false);

    SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &block_from, &block_to);

    offset = 0;
    SOS_buffer_pack(reply, &offset, "gg",
        block_from,
        block_to);

    i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
        SOSD_countof(socket_bytes_sent += i);
    }

    SOS_buffer_destroy(reply);

    return;
}


void SOSD_handle_announce(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_announce");
    SOSD_db_task   *task;
    SOS_msg_header  header;
    SOS_buffer     *reply;
    SOS_pub        *pub;
    
    char            pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int             offset;
    int             i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_ANNOUNCE\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    if (pub == NULL) {
        dlog(5, "     ... NOPE!  Adding new pub to the table.\n");
        /* If it's not in the table, add it. */
        pub = SOS_pub_create(SOS, pub_guid_str, SOS_NATURE_DEFAULT);
        SOSD_countof(pub_handles++);
        strncpy(pub->guid_str, pub_guid_str, SOS_DEFAULT_STRING_LEN);
        pub->guid = header.pub_guid;
        SOSD.pub_table->put(SOSD.pub_table, pub_guid_str, pub);
    } else {
        dlog(5, "     ... FOUND IT!\n");
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n", SOSD.pub_table->size(SOSD.pub_table));
    dlog(5, "Calling SOSD_apply_announce() ...\n");

    SOSD_apply_announce(pub, buffer);
    pub->announced = SOSD_PUB_ANN_DIRTY;

    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->pub = pub;
    task->type = SOS_MSG_TYPE_ANNOUNCE;
    pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
    pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
    SOSD.sync.db.queue->elem_count++;
    SOS_buffer_destroy(reply);
    pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);

    dlog(5, "  ... pub(%" SOS_GUID_FMT ")->elem_count = %d\n", pub->guid, pub->elem_count);

    return;
}


void SOSD_handle_publish(SOS_buffer *buffer)  {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_publish");
    SOSD_db_task   *task;
    SOS_msg_header  header;
    SOS_buffer     *reply;
    SOS_pub        *pub;
    char            pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int             offset;
    int             i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_PUBLISH\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    /* Check the table for this pub ... */
    dlog(5, "  ... checking SOS->pub_table for GUID(%s):\n", pub_guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, pub_guid_str);

    if (pub == NULL) {
        /* If it's not in the table, add it. */
    dlog(0, "ERROR: PUBLISHING INTO A PUB (guid:%" SOS_GUID_FMT ") NOT FOUND! (WEIRD!)\n", header.pub_guid);
    dlog(0, "ERROR: .... ADDING previously unknown pub to the table... (this is bogus, man)\n");
        pub = SOS_pub_create(SOS, pub_guid_str, SOS_NATURE_DEFAULT);
        SOSD_countof(pub_handles++);
        strncpy(pub->guid_str, pub_guid_str, SOS_DEFAULT_STRING_LEN);
        pub->guid = header.pub_guid;
        SOSD.pub_table->put(SOSD.pub_table, pub_guid_str, pub);
    } else {
        dlog(5, "     ... FOUND it!\n");
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n", SOSD.pub_table->size(SOSD.pub_table));

    SOSD_apply_publish(pub, buffer);

    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->pub = pub;
    task->type = SOS_MSG_TYPE_PUBLISH;
    pthread_mutex_lock(pub->lock);
    if (pub->sync_pending == 0) {
        pub->sync_pending = 1;
        pthread_mutex_unlock(pub->lock);
        pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
        pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
        SOSD.sync.db.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);
    } else {
        pthread_mutex_unlock(pub->lock);
    }

    // Spin off the k-means information to be treated
    //   in parallel with its database injection:
    if ((pub->meta.nature == SOS_NATURE_KMEAN_2D)
        && (SOS->role == SOS_ROLE_LISTENER)) {
        dlog(4, "Re-queing the k-means task for processing...\n");
        SOS_buffer *copy;
        // Make a copy of this buffer.
        SOS_buffer_clone(&copy, buffer); 
        // Set it to be the KMEAN type of data:
        offset = 0;
        header.msg_type = SOS_MSG_TYPE_KMEAN_DATA;
        SOS_buffer_pack(copy, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);
        // Enqueue it to be handled:
        pthread_mutex_lock(SOSD.sync.local.queue->sync_lock);
        pipe_push(SOSD.sync.local.queue->intake, (void *) &copy, 1);
        SOSD.sync.local.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.local.queue->sync_lock);
     }



    return;
}



void SOSD_handle_shutdown(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_shutdown");
    SOS_msg_header header;
    SOS_buffer    *reply;
    int            offset;
    int            i;

    dlog(1, "header.msg_type = SOS_MSG_TYPE_SHUTDOWN\n");

    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    if (SOS->role == SOS_ROLE_LISTENER) {
        SOSD_PACK_ACK(reply);
        
        i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
        if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        else {
            dlog(5, "  ... send() returned the following bytecount: %d\n", i);
            SOSD_countof(socket_bytes_sent += i);
        }
    }

    #if (SOSD_CLOUD_SYNC > 0)
    SOSD_cloud_shutdown_notice();
    #endif

    SOSD.daemon.running = 0;
    SOS->status = SOS_STATUS_SHUTDOWN;

    /*
     * We don't need to do this here, the handlers are called by the same thread as
     * the listener, so setting the flag (above) is sufficient to stop the listener
     * loop and initiate a clean shutdown.
     *
    shutdown(SOSD.net.server_socket_fd, SHUT_RDWR);
     *
     */

    SOS_buffer_destroy(reply);

    return;
}



void SOSD_handle_check_in(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_check_in");
    SOS_msg_header header;
    char           function_name[SOS_DEFAULT_STRING_LEN] = {0};
    SOS_buffer    *reply;
    int            offset;
    int            i;

    dlog(1, "header.msg_type = SOS_MSG_TYPE_CHECK_IN\n");

    SOSD_countof(feedback_checkin_messages++);

    SOS_buffer_init(SOS, &reply);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    if (SOS->role == SOS_ROLE_LISTENER) {
        /* Build a reply: */
        memset(&header, '\0', sizeof(SOS_msg_header));
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_FEEDBACK;
        header.msg_from = 0;
        header.pub_guid = 0;

        offset = 0;
        SOS_buffer_pack(reply, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);

        /* TODO: { FEEDBACK } Currently this is a hard-coded 'exec function' case. */
        snprintf(function_name, SOS_DEFAULT_STRING_LEN, "demo_function");

        SOS_buffer_pack(reply, &offset, "is",
            SOS_FEEDBACK_EXEC_FUNCTION,
            function_name);

        /* Go back and set the message length to the actual length. */
        header.msg_size = offset;
        offset = 0;
        SOS_buffer_pack(reply, &offset, "i", header.msg_size);

        dlog(1, "Replying to CHECK_IN with SOS_FEEDBACK_EXEC_FUNCTION(%s)...\n", function_name);

        i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
        if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        else {
            dlog(5, "  ... send() returned the following bytecount: %d\n", i); 
            SOSD_countof(socket_bytes_sent += i);
        }
        

    }

    SOS_buffer_destroy(reply);
    dlog(5, "Done!\n");

    return;
}



void SOSD_handle_probe(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_probe");
    SOS_buffer    *reply;
    int            i;

    SOS_buffer_init_sized_locking(SOS, &reply, SOS_DEFAULT_BUFFER_MAX, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_PROBE;
    header.msg_from = SOS->config.comm_rank;
    header.pub_guid = -1;

    int offset = 0;
    SOS_buffer_pack(reply, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.pub_guid);

    /* Don't need to lock for probing because it doesn't matter if we're a little off. */
    uint64_t queue_depth_local     = SOSD.sync.local.queue->elem_count;
    uint64_t queue_depth_cloud     = SOSD.sync.cloud.queue->elem_count;
    uint64_t queue_depth_db_tasks  = SOSD.sync.db.queue->elem_count;
    uint64_t queue_depth_db_snaps  = SOSD.db.snap_queue->elem_count;

    SOS_buffer_pack(reply, &offset, "gggg",
                    queue_depth_local,
                    queue_depth_cloud,
                    queue_depth_db_tasks,
                    queue_depth_db_snaps);


    SOSD_counts current;
    if (SOS_DEBUG > 0) { pthread_mutex_lock(SOSD.daemon.countof.lock_stats); }
    current = SOSD.daemon.countof;
    if (SOS_DEBUG > 0) { pthread_mutex_unlock(SOSD.daemon.countof.lock_stats); }

    SOS_buffer_pack(reply, &offset, "ggggggggggggggggggggg",
                    current.thread_local_wakeup,
                    current.thread_cloud_wakeup,
                    current.thread_db_wakeup,
                    current.feedback_checkin_messages,
                    current.socket_messages,
                    current.socket_bytes_recv,
                    current.socket_bytes_sent,
                    current.mpi_sends,
                    current.mpi_bytes,
                    current.db_transactions,
                    current.db_insert_announce,
                    current.db_insert_announce_nop,
                    current.db_insert_publish,
                    current.db_insert_publish_nop,
                    current.db_insert_val_snaps,
                    current.db_insert_val_snaps_nop,
                    current.buffer_creates,
                    current.buffer_bytes_on_heap,
                    current.buffer_destroys,
                    current.pipe_creates,
                    current.pub_handles);

    uint64_t vm_peak      = 0;
    uint64_t vm_size      = 0;

    FILE    *mf           = NULL;
    int      mf_rc        = 0;
    char     mf_path[128] = {0};
    char    *mf_line      = NULL;
    size_t   mf_line_len  = 0;

    sprintf(mf_path, "/proc/%d/status", getpid());
    mf = fopen(mf_path, "r");
    if (mf == NULL) {
        dlog(0, "ERROR: Could not open %s file for probe request.   (%s)\n", mf_path, strerror(errno));
        vm_peak    = 0;
        vm_size    = 0;
    } else {
        while((mf_rc = getline(&mf_line, &mf_line_len, mf)) != -1) {
            if (strncmp(mf_line, "VmPeak", 6) == 0) {
                sscanf(mf_line, "VmPeak: %" PRIu64 " *", &vm_peak);
            }
            if (strncmp(mf_line, "VmSize", 6) == 0) {
                sscanf(mf_line, "VmSize: %" PRIu64 " *", &vm_size);
            }
            if ((vm_peak > 0) && (vm_size > 0)) {
                break;
            }
        }
    }

    fclose(mf);

    SOS_buffer_pack(reply, &offset, "gg",
                    vm_peak,
                    vm_size);

    // Buffer is now ready to send...

    i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
        SOSD_countof(socket_bytes_sent += i);
    }

    SOS_buffer_destroy(reply);
    return;
}



void SOSD_handle_unknown(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_unknown");
    SOS_msg_header  header;
    SOS_buffer     *reply;
    int             offset;
    int             i;

    dlog(1, "header.msg_type = UNKNOWN\n");

    SOS_buffer_init(SOS, &reply);

    offset = 0;
    SOS_buffer_unpack(reply, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    dlog(1, "header.msg_size == %d\n", header.msg_size);
    dlog(1, "header.msg_type == %d\n", header.msg_type);
    dlog(1, "header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(1, "header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);

    if (SOS->role == SOS_ROLE_AGGREGATOR) {
        SOS_buffer_destroy(reply);
        return;
    }

    SOSD_PACK_ACK(reply);

    i = send( SOSD.net.client_socket_fd, (void *) reply->data, reply->len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
        SOSD_countof(socket_bytes_sent += i);
    }

    SOS_buffer_destroy(reply);

    return;
}



void SOSD_setup_socket() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_setup_socket");
    int i;
    int yes;
    int opts;

    yes = 1;

    memset(&SOSD.net.server_hint, '\0', sizeof(struct addrinfo));
    SOSD.net.server_hint.ai_family     = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    SOSD.net.server_hint.ai_socktype   = SOCK_STREAM;   /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
    SOSD.net.server_hint.ai_flags      = AI_PASSIVE;    /* For wildcard IP addresses */
    SOSD.net.server_hint.ai_protocol   = 0;             /* Any protocol */
    SOSD.net.server_hint.ai_canonname  = NULL;
    SOSD.net.server_hint.ai_addr       = NULL;
    SOSD.net.server_hint.ai_next       = NULL;

    i = getaddrinfo(NULL, SOSD.net.server_port, &SOSD.net.server_hint, &SOSD.net.result);
    if (i != 0) { dlog(0, "Error!  getaddrinfo() failed. (%s) Exiting daemon.\n", strerror(errno)); exit(EXIT_FAILURE); }

    for ( SOSD.net.server_addr = SOSD.net.result ; SOSD.net.server_addr != NULL ; SOSD.net.server_addr = SOSD.net.server_addr->ai_next ) {
        dlog(1, "Trying an address...\n");

        SOSD.net.server_socket_fd = socket(SOSD.net.server_addr->ai_family, SOSD.net.server_addr->ai_socktype, SOSD.net.server_addr->ai_protocol );
        if ( SOSD.net.server_socket_fd < 1) {
            dlog(0, "  ... failed to get a socket.  (%s)\n", strerror(errno));
            continue;
        }

        /*
         *  Allow this socket to be reused/rebound quickly by the daemon.
         */
        if ( setsockopt( SOSD.net.server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            dlog(0, "  ... could not set socket options.  (%s)\n", strerror(errno));
            continue;
        }

        if ( bind( SOSD.net.server_socket_fd, SOSD.net.server_addr->ai_addr, SOSD.net.server_addr->ai_addrlen ) == -1 ) {
            dlog(0, "  ... failed to bind to socket.  (%s)\n", strerror(errno));
            close( SOSD.net.server_socket_fd );
            continue;
        } 
        /* If we get here, we're good to stop looking. */
        break;
    }

    if ( SOSD.net.server_socket_fd < 0 ) {
        dlog(0, "  ... could not socket/setsockopt/bind to anything in the result set.  last errno = (%d:%s)\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... got a socket, and bound to it!\n");
    }

    freeaddrinfo(SOSD.net.result);

    /*
     *   Enforce that this is a BLOCKING socket:
     */
    opts = fcntl(SOSD.net.server_socket_fd, F_GETFL);
    if (opts < 0) { dlog(0, "ERROR!  Cannot call fcntl() on the server_socket_fd to get its options.  Carrying on.  (%s)\n", strerror(errno)); }
 
    opts = opts & !(O_NONBLOCK);
    i    = fcntl(SOSD.net.server_socket_fd, F_SETFL, opts);
    if (i < 0) { dlog(0, "ERROR!  Cannot use fcntl() to set the server_socket_fd to BLOCKING more.  Carrying on.  (%s).\n", strerror(errno)); }


    listen( SOSD.net.server_socket_fd, SOSD.net.listen_backlog );
    dlog(0, "Listening on socket.\n");

    return;
}
 


void SOSD_init() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_init");
    pid_t pid, ppid, sid;
    int rc;

    /* [daemon name]
     *     assign a name appropriate for whether it is participating in a cloud or not
     */
    switch (SOS->role) {
    case SOS_ROLE_LISTENER:  snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME /* ".mon" */); break;
    case SOS_ROLE_AGGREGATOR:      snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME /* ".dat" */); break;
    default: break;
    }

    /* [lock file]
     *     create and hold lock file to prevent multiple daemon spawn
     */
    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.daemon.lock_file, SOS_DEFAULT_STRING_LEN, "%s/%s.%05d.lock", SOSD.daemon.work_dir, SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.daemon.lock_file, SOS_DEFAULT_STRING_LEN, "%s/%s.local.lock", SOSD.daemon.work_dir, SOSD.daemon.name);
    #endif
    sos_daemon_lock_fptr = open(SOSD.daemon.lock_file, O_RDWR | O_CREAT, 0640);
    if (sos_daemon_lock_fptr < 0) {
        fprintf(stderr, "\nERROR!  Unable to start daemon (%s): Could not access lock file %s in directory %s\n", SOSD.daemon.name, SOSD.daemon.lock_file, SOSD.daemon.work_dir);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    if (lockf(sos_daemon_lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\nERROR!  Unable to start daemon (%s): AN INSTANCE IS ALREADY RUNNING!\n", SOSD.daemon.name);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("Lock file obtained.  (%s)\n", SOSD.daemon.lock_file); fflush(stdout); }


    /* [log file]
     *      system logging initialize
     */
    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN, "%s/%s.%05d.log", SOSD.daemon.work_dir, SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN, "%s/%s.local.log", SOSD.daemon.work_dir, SOSD.daemon.name);
    #endif
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("Opening log file: %s\n", SOSD.daemon.log_file); fflush(stdout); }
    sos_daemon_log_fptr = fopen(SOSD.daemon.log_file, "w"); /* Open a log file, even if we don't use it... */
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("  ... done.\n"); fflush(stdout); }



    if (!SOSD_ECHO_TO_STDOUT) {
        dlog(1, "Logging output up to this point has been suppressed, but all initialization has gone well.\n");
        dlog(1, "Log file is now open.  Proceeding...\n");
        dlog(1, "SOSD_init():\n");
    }

    /* [mode]
     *      interactive or detached/daemon
     */
    #if (SOSD_DAEMON_MODE > 0)
    {
    dlog(1, "  ...mode: DETACHED DAEMON (fork/umask/sedsid)\n");
        /* [fork]
         *     split off from the parent process (& terminate parent)
         */
        ppid = getpid();
        pid  = fork();
        
        if (pid < 0) {
            dlog(0, "ERROR! Unable to start daemon (%s): Could not fork() off parent process.\n", SOSD.daemon.name);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent
        
        /* [child session]
         *     create/occupy independent session from parent process
         */
        umask(0);
        sid = setsid();
        if (sid < 0) {
            dlog(0, "ERROR!  Unable to start daemon (%s): Could not acquire a session id.\n", SOSD_DAEMON_NAME); 
            exit(EXIT_FAILURE);
        }
        if ((chdir(SOSD.daemon.work_dir)) < 0) {
            dlog(0, "ERROR!  Unable to start daemon (%s): Could not change to working directory: %s\n", SOSD_DAEMON_NAME, SOSD.daemon.work_dir);
            exit(EXIT_FAILURE);
        }

        /* [file handles]
         *     close unused IO handles
         */
        if (SOS_DEBUG < 1) {
            dlog(1, "Closing traditional I/O for the daemon...\n");
            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);
        }

        
        dlog(1, "  ... session(%d) successfully split off from parent(%d).\n", getpid(), ppid);
    }
    #else
    {
        dlog(1, "  ... mode: ATTACHED INTERACTIVE\n");
    }
    #endif

    sprintf(SOSD.daemon.pid_str, "%d", getpid());
    dlog(1, "  ... pid: %s\n", SOSD.daemon.pid_str);

    /* Now we can write our PID out to the lock file safely... */
    rc = write(sos_daemon_lock_fptr, SOSD.daemon.pid_str, strlen(SOSD.daemon.pid_str));



    /* [guid's]
     *     configure the issuer of guids for this daemon
     */
    dlog(1, "Obtaining this instance's guid range...\n");
    #if (SOSD_CLOUD_SYNC > 0)
        SOS_guid guid_block_size = (SOS_guid) (SOS_DEFAULT_UID_MAX / (SOS_guid) SOS->config.comm_size);
        SOS_guid guid_my_first   = (SOS_guid) SOS->config.comm_rank * guid_block_size;
        //printf("%d: My guid range: %" SOS_GUID_FMT " - %" SOS_GUID_FMT, SOS->config.comm_rank, guid_my_first, (guid_my_first + (guid_block_size - 1))); fflush(stdout);
        SOS_uid_init(SOS, &SOSD.guid, guid_my_first, (guid_my_first + (guid_block_size - 1)));
    #else
        dlog(1, "DATA NOTE:  Running in local mode, CLOUD_SYNC is disabled.\n");
        dlog(1, "DATA NOTE:  GUID values are unique only to this node.\n");
        SOS_uid_init(&SOSD.guid, 1, SOS_DEFAULT_UID_MAX);
    #endif
    dlog(1, "  ... (%" SOS_GUID_FMT " ---> %" SOS_GUID_FMT ")\n", SOSD.guid->next, SOSD.guid->last);

    /* [hashtable]
     *    storage system for received pubs.  (will enque their key -> db)
     */
    dlog(1, "Setting up a hash table for pubs...\n");
    SOSD.pub_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(1, "Daemon initialization is complete.\n");
    SOSD.daemon.running = 1;
    return;
}



 void SOSD_sync_context_init(SOS_runtime *sos_context, SOSD_sync_context *sync_context, size_t elem_size, void* (*thread_func)(void *thread_param)) {
    SOS_SET_CONTEXT(sos_context, "SOSD_sync_context_init");

    sync_context->sos_context = sos_context;
    SOS_pipe_init(SOS, &sync_context->queue, elem_size);
    sync_context->handler = (pthread_t *) malloc(sizeof(pthread_t));
    sync_context->lock    = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    sync_context->cond    = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));

    pthread_mutex_init(sync_context->lock, NULL);
    pthread_cond_init(sync_context->cond, NULL);
    pthread_create(sync_context->handler, NULL, thread_func, (void *) sync_context);

    return;
}


void SOSD_claim_guid_block(SOS_uid *id, int size, SOS_guid *pool_from, SOS_guid *pool_to) {
    SOS_SET_CONTEXT(id->sos_context, "SOSD_claim_guid_block");

    pthread_mutex_lock( id->lock );

    if ((id->next + size) > id->last) {
        /* This is basically a failure case if any more GUIDs are requested. */
        *pool_from = id->next;
        *pool_to   = id->last;
        id->next   = id->last + 1;
    } else {
        *pool_from = id->next;
        *pool_to   = id->next + size;
        id->next   = id->next + size + 1;
        dlog(0, "served GUID block: %" SOS_GUID_FMT " ----> %" SOS_GUID_FMT "\n",
             *pool_from, *pool_to);
    }

    pthread_mutex_unlock( id->lock );

    return;
}


void SOSD_apply_announce( SOS_pub *pub, SOS_buffer *buffer ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_announce");

    dlog(6, "Calling SOS_announce_from_buffer()...\n");
    SOS_announce_from_buffer(buffer, pub);

    return;
}


void SOSD_apply_publish( SOS_pub *pub, SOS_buffer *buffer ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_publish");

    dlog(6, "Calling SOS_publish_from_buffer()...\n");
    SOS_publish_from_buffer(buffer, pub, SOSD.db.snap_queue);

    return;
}





void SOSD_display_logo(void) {

    int choice = 0;

    //srand(getpid());
    //choice = rand() % 3;

    printf("--------------------------------------------------------------------------------\n");
    printf("\n");

    switch (choice) {
    case 0:
        printf("           _/_/_/    _/_/      _/_/_/ ||[]||[]}))))}][]||||  Scalable\n");
        printf("        _/        _/    _/  _/        |||||[][{(((({[]|[]||  Observation\n");
        printf("         _/_/    _/    _/    _/_/     |[][]|[]}))))}][]||[]  System\n");
        printf("            _/  _/    _/        _/    []|||[][{(((({[]|[]||  for Scientific\n");
        printf("     _/_/_/      _/_/    _/_/_/       ||[]||[]}))))}][]||[]  Workflows\n");
        break;


    case 1:
        printf("               _/_/_/    _/_/      _/_/_/   |[]}))))}][]    Scalable\n");
        printf("            _/        _/    _/  _/          [][{(((({[]|    Observation\n");
        printf("             _/_/    _/    _/    _/_/       |[]}))))}][]    System\n");
        printf("                _/  _/    _/        _/      [][{(((({[]|    for Scientific\n");
        printf("         _/_/_/      _/_/    _/_/_/         |[]}))))}][]    Workflows\n");
        break;

    case 2:
        printf("               ._____________________________________________________ ___  _ _\n");
        printf("              /_____/\\/\\/\\/\\/\\____/\\/\\/\\/\\______/\\/\\/\\/\\/\\________ ___ _ _\n");
        printf("             /___/\\/\\__________/\\/\\____/\\/\\__/\\/\\_______________ ___  _   __ _\n");
        printf("            /_____/\\/\\/\\/\\____/\\/\\____/\\/\\____/\\/\\/\\/\\_________  _ __\n");
        printf("           /___________/\\/\\__/\\/\\____/\\/\\__________/\\/\\_______ ___ _  __ _   _\n");
        printf("          /___/\\/\\/\\/\\/\\______/\\/\\/\\/\\____/\\/\\/\\/\\/\\_________ ___ __ _  _   _\n");
        printf("         /_____________________________________________________ _  _      _\n");
        printf("        /___/\\/\\/\\__/\\/\\___________________________________ ___ _ __ __  _    _\n");
        printf("       /___/\\/\\______/\\/\\______/\\/\\/\\____/\\/\\______/\\/\\___ ___ _ ___ __ _  _\n");
        printf("      /___/\\/\\/\\____/\\/\\____/\\/\\__/\\/\\__/\\/\\__/\\__/\\/\\___ __ _ _  _  _  _\n");
        printf("     /___/\\/\\______/\\/\\____/\\/\\__/\\/\\__/\\/\\/\\/\\/\\/\\/\\____ __ ___  _\n");
        printf("    /___/\\/\\______/\\/\\/\\____/\\/\\/\\______/\\/\\__/\\/\\_____ ___     _\n");
        printf("   |__________________________________________________ ___ _  _ _  ___  _\n");
        printf("\n");
        printf("   * * *   Scalable Observation System for Scientific Workflows   * * *\n");
        break;


    case 3:
        printf("_._____. .____  .____.._____. _  ..\n");
        printf("__  ___/_  __ \\_  ___/__  __/__  /________      __    .:|   Scalable\n");
        printf(".____ \\_  / / /.___ \\__  /_ ._  /_  __ \\_ | /| / /    .:|   Observation\n");
        printf("____/ // /_/ /.___/ /_  __/ _  / / /_/ /_ |/ |/ /     .:|   System for Scientific\n");
        printf("/____/ \\____/ /____/ /_/    /_/  \\____/____/|__/      .:|   Workflows\n");
        break;

    }

    printf("\n");
    printf("   Version: %s\n", SOS_VERSION);
    printf("   Builder: %s\n", SOS_BUILDER);
    printf("\n");
    printf("--------------------------------------------------------------------------------\n");

    return;
}
