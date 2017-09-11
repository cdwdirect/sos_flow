/*
 *  sosd.c (daemon)
 *
 *
 *
 */

// ############################################################################

#ifndef SOSD_CLOUD_SYNC
    #define OPT_PARAMS ""\
        "\n" \
        "                 The following parameters are REQUIRED for"\
                " STANDALONE operation:\n" \
        "\n" \
        "                 -k, --rank <rank within ALL sosd instances>\n" \
        "\n"
 
#endif

#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
    #define OPT_PARAMS ""\
        "\n" \
        "                 The following parameters are REQUIRED for"\
                " EVPath:\n" \
        "\n" \
        "                 -k, --rank <rank within ALL sosd instances>\n" \
        "                 -r, --role <listener | aggregator>\n" \
        "\n" \
        "                 NOTE: Aggregator ranks [-k #] need to be contiguous"\
                " from 0 to n-1 aggregators.\n" \
        "\n" \
        "\n" 
#else
    #define OPT_PARAMS "\n"
#endif


#define USAGE          "USAGE:   $ sosd  -l, --listeners <count>\n" \
                       "                 -a, --aggregators <count>\n" \
                       "                [-w, --work_dir <full_path>]\n" \
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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


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
    int        elem;
    int        next_elem;
    int        retval;
    SOS_role   my_role;
    int        my_rank;

    /* [countof]
     *    statistics for daemon activity.
     */
    memset(&SOSD.daemon.countof, 0, sizeof(SOSD_counts));
    if (SOS_DEBUG > 0) {
        SOSD.daemon.countof.lock_stats =
                (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(SOSD.daemon.countof.lock_stats, NULL);
    }

    SOSD.daemon.work_dir    = (char *) calloc(sizeof(char), PATH_MAX); 
    SOSD.daemon.name        = (char *) calloc(sizeof(char), PATH_MAX);
    SOSD.daemon.lock_file   = (char *) calloc(sizeof(char), PATH_MAX);
    SOSD.daemon.log_file    = (char *) calloc(sizeof(char), PATH_MAX);

    // Default the working directory to the current working directory,
    // can be overridden at the command line with the -w option.
    if (!getcwd(SOSD.daemon.work_dir, PATH_MAX)) {
        fprintf(stderr, "STATUS: The getcwd() function did not succeed, make"
                " sure to provide a -w <path> command line option.\n");
    }

    my_role = SOS_ROLE_UNASSIGNED;
    my_rank = -1;

    //NOTE: This is duplicated from SOS_target_init() since we're starting
    // up by initializing all this stuff manually before even SOS_init()
    SOS_socket *tgt = (SOS_socket *) calloc(1, sizeof(SOS_socket)); 
    SOSD.net = tgt;

    tgt->send_lock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_lock(tgt->send_lock);

    // Grab the port from the environment variable SOS_CMD_PORT
    strncpy(tgt->local_host, SOS_DEFAULT_SERVER_HOST, NI_MAXHOST);
    strncpy(tgt->local_port, getenv("SOS_CMD_PORT"), NI_MAXSERV);
    if ((tgt->local_port == NULL) || (strlen(tgt->local_port)) < 2) {
        fprintf(stderr, "STATUS: SOS_CMD_PORT evar not set.  Using default: %s\n",
                SOS_DEFAULT_SERVER_PORT);
        fflush(stderr);
        strncpy(tgt->local_port, SOS_DEFAULT_SERVER_PORT, NI_MAXSERV);
    }
    tgt->port_number = atoi(tgt->local_port);

    tgt->listen_backlog = 20;
    tgt->buffer_len                = SOS_DEFAULT_BUFFER_MAX;
    tgt->timeout                   = SOS_DEFAULT_MSG_TIMEOUT;
    tgt->local_hint.ai_family     = AF_UNSPEC;     // Allow IPv4 or IPv6
    tgt->local_hint.ai_socktype   = SOCK_STREAM;   // _STREAM/_DGRAM/_RAW
    tgt->local_hint.ai_flags      = AI_NUMERICSERV;// Don't invoke namserv.
    tgt->local_hint.ai_protocol   = 0;             // Any protocol
    pthread_mutex_unlock(tgt->send_lock); 
    // --- end duplication of SOS_target_init();

    /* Process command-line arguments */
#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
    if ( argc < 9 ) {
#else
    if ( argc < 5 ) {
#endif
        fprintf(stderr, "ERROR: Invalid number of arguments supplied."
                "   (%d)\n\n%s\n", argc, USAGE);
        exit(EXIT_FAILURE);
    }
    //SOSD.net.listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "ERROR: Incorrect parameter"
                    " pairing.\n\n%s\n", USAGE);
            exit(EXIT_FAILURE);
        }

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
            free(SOSD.daemon.work_dir); // Default getcwd() string.
            SOSD.daemon.work_dir    = argv[next_elem];
        }
#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
        else if ( (strcmp(argv[elem], "--rank"            ) == 0)
        ||        (strcmp(argv[elem], "-k"                ) == 0)) {
            my_rank = atoi(argv[next_elem]);
        }
        else if ( (strcmp(argv[elem], "--role"            ) == 0)
        ||        (strcmp(argv[elem], "-r"                ) == 0)) {
            SOSD.daemon.evpath.instance_role = argv[next_elem];
            if (strcmp(argv[next_elem], "listener") == 0) {
                my_role = SOS_ROLE_LISTENER;
            } else if (strcmp(argv[next_elem], "aggregator") == 0) {
                my_role = SOS_ROLE_AGGREGATOR;
            } else {
                fprintf(stderr, "ERROR!  Invalid sosd role specified.  (%s)\n",
                    argv[next_elem]);
                exit(EXIT_FAILURE);
            }
            fflush(stdout);
        }
#endif
        else    { fprintf(stderr, "Unknown flag: %s %s\n",
                    argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }

    if ((SOSD.net->port_number < 1) && (my_role == SOS_ROLE_UNASSIGNED)) {
        fprintf(stderr, "ERROR: No port was specified for the daemon"
                " to monitor.\n\n%s\n", USAGE);
        exit(EXIT_FAILURE);
    }

    #ifndef SOSD_CLOUD_SYNC
    if (my_role != SOS_ROLE_LISTENER) {
        printf("NOTE: Terminating an instance of sosd with pid: %d\n", getpid());
        printf("NOTE: SOSD_CLOUD_SYNC is disabled but this instance"
                " is not a SOS_ROLE_LISTENER!\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    #endif

    #ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
    if (SOSD.daemon.evpath.instance_role == NULL) {
        fprintf(stderr, "ERROR: Please select an instance role.\n%s\n", USAGE);
        exit(EXIT_FAILURE);
    }

    if (my_rank < 0) {
        fprintf(stderr, "ERROR: No rank was assigned for this"
                " daemon.\n\n%s\n", USAGE);
        exit(EXIT_FAILURE);
    }

    #endif

    // Done with param processing... fire things up.

    memset(&SOSD.daemon.pid_str, '\0', 256);

    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) {
        printf("Preparing to initialize:\n");
        fflush(stdout);
    }

    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) {
        printf("   ... working directory: %s:\n", SOSD.daemon.work_dir);
        fflush(stdout);
    }

    SOSD.sos_context = (SOS_runtime *) malloc(sizeof(SOS_runtime));
    memset(SOSD.sos_context, '\0', sizeof(SOS_runtime));

    SOSD.sos_context->role              = my_role;
    SOSD.sos_context->config.comm_rank  = my_rank;

    #ifdef SOSD_CLOUD_SYNC
    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) {
        printf("   ... calling SOSD_cloud_init()...\n"); fflush(stdout);
    }
    // Start the msg_recv loop before anything, to handle registration replies
    // between the aggregator and the listeners. It has an internal gate that
    // does not open until the bare minimum objects are in place halfway
    // through the SOSD_cloud_init routine.
    SOSD_cloud_init( &argc, &argv);
    SOSD_sync_context_init(SOSD.sos_context, &SOSD.sync.cloud_recv,
            sizeof(SOS_buffer *), SOSD_THREAD_cloud_recv);
    #else
    dlog(0, "   ... WARNING: There is no CLOUD_SYNC configured"
            " for this SOSD.\n");
    #endif

    SOS_SET_CONTEXT(SOSD.sos_context, "main");

    my_role = SOS->role;

    dlog(0, "Initializing SOSD:\n");
    dlog(0, "   ... calling SOS_init(argc, argv, %s, SOSD.sos_context)"
            " ...\n", SOS_ENUM_STR( SOS->role, SOS_ROLE ));
    SOS_init_existing_runtime( &argc, &argv, &SOSD.sos_context,
            my_role, SOS_RECEIVES_NO_FEEDBACK, NULL);

    dlog(0, "   ... calling SOSD_init()...\n");
    SOSD_init();
    dlog(0, "   ... done. (SOSD_init + SOS_init are complete)\n");

    if (SOS->config.comm_rank == 0) {
        SOSD_display_logo();
    }

    dlog(0, "Calling register_signal_handler()...\n");
    if (SOSD_DAEMON_LOG > -1) SOS_register_signal_handler(SOSD.sos_context);

    dlog(0, "Calling daemon_setup_socket()...\n");
    SOSD_setup_socket();
    SOSD.net->sos_context = SOSD.sos_context;

    dlog(0, "Calling daemon_init_database()...\n");
    SOSD_db_init_database();

    dlog(0, "Initializing the sync framework...\n");

    SOSD_sync_context_init(SOS, &SOSD.sync.db,
            sizeof(SOSD_db_task *), SOSD_THREAD_db_sync);
    
    #ifdef SOSD_CLOUD_SYNC
    if (SOS->role == SOS_ROLE_LISTENER) {
        SOSD_sync_context_init(SOS, &SOSD.sync.cloud_send,
                sizeof(SOS_buffer *), SOSD_THREAD_cloud_send);
        SOSD_cloud_start();
    }
    #else
    #endif
    SOSD_sync_context_init(SOS, &SOSD.sync.local, sizeof(SOS_buffer *),
        SOSD_THREAD_local_sync);
    // do system monitoring, if requested.
    char *system_flag = getenv("SOS_READ_SYSTEM_STATUS");
    if ((system_flag != NULL) && (strlen(system_flag)) > 0) {
        //setup_system_monitor_pub();
        SOSD_sync_context_init(SOS, &SOSD.sync.system_monitor, 0,
             SOSD_THREAD_system_monitor);
        SOSD.system_monitoring = 1;
    } else {
        SOSD.system_monitoring = 0;
    }

    SOSD_sync_context_init(SOS, &SOSD.sync.feedback,
            sizeof(SOSD_feedback_task *), SOSD_THREAD_feedback_sync);

    SOSD.sync.sense_list_lock = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(SOSD.sync.sense_list_lock, NULL);

    dlog(0, "Entering listening loops...\n");

    switch (SOS->role) {
    case SOS_ROLE_LISTENER:
        SOS->config.locale = SOS_LOCALE_APPLICATION;
        break;
    case SOS_ROLE_AGGREGATOR:
        SOS->config.locale = SOS_LOCALE_DAEMON_DBMS;
        break;
    default: break;
    }

    //
    //
    // Start listening to the socket with the main thread:
    //
    SOSD_listen_loop();
    //
    // Wait for the database to be done flushing...
    //
    pthread_join(*SOSD.sync.db.handler, NULL);
    //
    // Done!  Cleanup and shut down.
    //
    //

    
    dlog(0, "Closing the sync queues:\n");
    
    if (SOSD.sync.local.queue != NULL) {
        dlog(0, "  .. SOSD.sync.local.queue\n");
        pipe_producer_free(SOSD.sync.local.queue->intake);
    }
    if (SOSD.sync.cloud_send.queue != NULL) {
        dlog(0, "  .. SOSD.sync.cloud_send.queue\n");
        pipe_producer_free(SOSD.sync.cloud_send.queue->intake);
    }
    if (SOSD.sync.cloud_recv.queue != NULL) {
        dlog(0, "  .. SOSD.sync.cloud_recv.queue\n");
        pipe_producer_free(SOSD.sync.cloud_recv.queue->intake);
    }
    if (SOSD.sync.db.queue != NULL) {
        dlog(0, "  .. SOSD.sync.db.queue\n");
        pipe_producer_free(SOSD.sync.db.queue->intake);
    }
    if (SOSD.sync.feedback.queue != NULL) {
        dlog(0, "  .. SOSD.sync.feedback.queue\n");
        pipe_producer_free(SOSD.sync.feedback.queue->intake);
    }

    //Clean up the sensitivity lists:
    pthread_mutex_lock(SOSD.sync.sense_list_lock);
    SOSD_sensitivity_entry *entry = SOSD.sync.sense_list_head;
    SOSD_sensitivity_entry *next_entry;
    while (entry != NULL) {
        next_entry = entry->next_entry;
        free(entry->sense_handle);
        free(entry->remote_host);
        SOS_target_destroy(entry->target);
        free(entry);
        entry = next_entry;
    }
    pthread_mutex_destroy(SOSD.sync.sense_list_lock);
    free(SOSD.sync.sense_list_lock);

    SOS->status = SOS_STATUS_HALTING;
    SOSD.db.ready = -1;

    dlog(0, "Destroying uid configurations.\n");
    SOS_uid_destroy( SOSD.guid );
    dlog(0, "  ... done.\n");
    dlog(0, "Closing the database.\n");
    SOSD_db_close_database();
    dlog(0, "Closing the socket.\n");
    shutdown(SOSD.net->local_socket_fd, SHUT_RDWR);
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

    fprintf(stdout, "** SHUTDOWN **: sosd(%d) is exiting cleanly!\n", my_rank);
    fflush(stdout);

    return(EXIT_SUCCESS);
} //end: main()



// The main loop for listening to the on-node socket.
// Messages received here are placed in the local_sync queue
//     for processing, so we can go back and grab the next
//     socket message ASAP.
void SOSD_listen_loop() {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_listen_loop");
    SOS_msg_header header;
    SOS_buffer    *buffer;
    SOS_buffer    *rapid_reply;
    int            recv_len;
    int            offset;
    int            i;

    buffer = NULL;
    rapid_reply = NULL;
    SOS_buffer_init_sized_locking(SOS, &buffer,
            SOS_DEFAULT_BUFFER_MAX, false);
    SOS_buffer_init_sized_locking(SOS, &rapid_reply,
            SOS_DEFAULT_BUFFER_MAX, false);
    
    dlog(5, "Assembling rapid_reply for val_snaps...\n");
    SOSD_PACK_ACK(rapid_reply);

    dlog(0, "Entering main loop...\n");
    while (SOSD.daemon.running) {
        SOS_buffer_wipe(buffer);

        //dlog(5, "Listening for a message...\n");
        //SOSD.net->peer_addr_len = sizeof(SOSD.net->peer_addr);
        //SOSD.net->remote_socket_fd = accept(SOSD.net->local_socket_fd,
        //        (struct sockaddr *) &SOSD.net->peer_addr,
        //        &SOSD.net->peer_addr_len);
        //i = getnameinfo((struct sockaddr *) &SOSD.net->peer_addr,
        //        SOSD.net->peer_addr_len, SOSD.net->remote_host,
        //        NI_MAXHOST, SOSD.net->remote_port, NI_MAXSERV,
        //        NI_NUMERICSERV);
        //if (i != 0) {
        //    dlog(0, "Error calling getnameinfo() on client connection."
        //            "  (%s)\n", strerror(errno));
        //    break;
        //}

        SOS_target_accept_connection(SOSD.net);

        dlog(5, "Accepted connection.  Attempting to receive message...\n");
        i = SOS_target_recv_msg(SOSD.net, buffer);
        if (i < sizeof(SOS_msg_header)) {
            SOS_target_disconnect(SOSD.net);
            continue;
        }

        offset = 0;
        SOS_msg_unzip(buffer, &header, 0, &offset);

        dlog(5, "Received connection.\n");
        dlog(5, "  ... msg_size == %d         (buffer->len == %d)\n",
                header.msg_size, buffer->len);
        dlog(5, "  ... msg_type == %s\n", SOS_ENUM_STR(header.msg_type,
                SOS_MSG_TYPE));

        if ((header.msg_size != buffer->len)
            || (header.msg_size > buffer->max)) {
            dlog(0, "ERROR:  BUFFER not correctly sized!"
                    "  header.msg_size == %d, buffer->len == %d / %d\n",
                     header.msg_size, buffer->len, buffer->max);
        }

        dlog(5, "  ... msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
        dlog(5, "  ....ref_guid == %" SOS_GUID_FMT "\n", header.ref_guid);

        SOSD_countof(socket_messages++);
        SOSD_countof(socket_bytes_recv += header.msg_size);

        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER:   SOSD_handle_register   (buffer); break;
        case SOS_MSG_TYPE_UNREGISTER: SOSD_handle_unregister (buffer); break;
        case SOS_MSG_TYPE_GUID_BLOCK: SOSD_handle_guid_block (buffer); break;

        case SOS_MSG_TYPE_ANNOUNCE:
        case SOS_MSG_TYPE_PUBLISH:
        case SOS_MSG_TYPE_VAL_SNAPS:
            pthread_mutex_lock(SOSD.sync.local.queue->sync_lock);
            pipe_push(SOSD.sync.local.queue->intake, (void *) &buffer, 1);
            SOSD.sync.local.queue->elem_count++;
            pthread_mutex_unlock(SOSD.sync.local.queue->sync_lock);
            buffer = NULL;
            SOS_buffer_init_sized_locking(SOS, &buffer,
                    SOS_DEFAULT_BUFFER_MAX, false);
            dlog(5, "  ... sending ACK w/reply->len == %d\n", rapid_reply->len);

            i = send( SOSD.net->remote_socket_fd, (void *) rapid_reply->data,
                    rapid_reply->len, 0);

            if (i == -1) {
                dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
            } else {
                dlog(5, "  ... send() returned the following"
                        " bytecount: %d\n", i);
                SOSD_countof(socket_bytes_sent += i);
            }    
            dlog(5, "  ... Done.\n");
            break;

        case SOS_MSG_TYPE_ECHO:        SOSD_handle_echo        (buffer); break;
        case SOS_MSG_TYPE_SHUTDOWN:    SOSD_handle_shutdown    (buffer); break;
        case SOS_MSG_TYPE_CHECK_IN:    SOSD_handle_check_in    (buffer); break;
        case SOS_MSG_TYPE_PROBE:       SOSD_handle_probe       (buffer); break;
        case SOS_MSG_TYPE_QUERY:       SOSD_handle_query       (buffer); break;
        case SOS_MSG_TYPE_SENSITIVITY: SOSD_handle_sensitivity (buffer); break;
        case SOS_MSG_TYPE_DESENSITIZE: SOSD_handle_desensitize (buffer); break;
        case SOS_MSG_TYPE_TRIGGERPULL: SOSD_handle_triggerpull (buffer); break;
        default:                       SOSD_handle_unknown     (buffer); break;
        }

        SOS_target_disconnect(SOSD.net);
    }

    SOS_buffer_destroy(buffer);
    SOS_buffer_destroy(rapid_reply);

    dlog(1, "Leaving the socket listening loop.\n");

    return;
}

/* -------------------------------------------------- */

void* SOSD_THREAD_system_monitor(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_system_monitor");
    struct timeval   now;
    struct timespec  wait;
    int rc;
    int period_seconds = 1;

    char *system_flag = getenv("SOS_READ_SYSTEM_STATUS");
    if ((system_flag != NULL) && (strlen(system_flag)) > 0) {
        period_seconds = atoi(system_flag);
    }

    SOSD_setup_system_data();
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_mutex_lock(my->lock);
        gettimeofday(&now, NULL);
        wait.tv_sec  = period_seconds + (now.tv_sec);
        wait.tv_nsec = (1000 * now.tv_usec);
        rc = pthread_cond_timedwait(my->cond, my->lock, &wait);
        pthread_mutex_unlock(my->lock);
        /* if we timed out, measure the system health. */
        switch (rc) {
            case 0:
                /* we were interrupted by the main thread. exit. */
                printf("Interrupted. Thread exiting...\n");
                break;
            case ETIMEDOUT:
                /* parse the system health */
                //printf("Reading system health...\n");
                SOSD_read_system_data();
                continue;
            case EINVAL:
            case EPERM:
            default:
                /* some other error. exit. */
                printf("Some other error. Thread exiting...\n");
                break;
        }
    }
    pthread_exit(NULL);
}

void* SOSD_THREAD_feedback_sync(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_feedback_sync");
    struct timeval   now;
    struct timespec  wait;
    SOS_msg_header   header;
    SOS_buffer      *buffer;
    int              offset;
    int              count;
    int              rc;

    pthread_mutex_lock(my->lock);

    // For processing queries...
    SOSD_query_handle *query;
    SOS_buffer *results_reply_msg = NULL;
    SOS_buffer_init(SOS, &results_reply_msg);
    // For processing payloads...
    SOSD_feedback_payload *payload = NULL;
    SOS_buffer *delivery = NULL;
    SOS_buffer_init_sized_locking(SOS, &delivery, 1024, false);


    gettimeofday(&now, NULL);
    wait.tv_sec  = SOSD_FEEDBACK_SYNC_WAIT_SEC  + (now.tv_sec);
    wait.tv_nsec = SOSD_FEEDBACK_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
    while (SOS->status == SOS_STATUS_RUNNING) {
        pthread_cond_timedwait(my->cond, my->lock, &wait);
        
        SOSD_feedback_task   *task;

        SOSD_countof(thread_feedback_wakeup++);

        // Grab the next feedback task...
        // This will block until a task is available or until the
        // pipe is closed.
        count = pipe_pop_eager(my->queue->outlet, (void *) &task, 1);
        if (count == 0) {
            dlog(6, "Nothing remains in the queue, and the intake"
            " is closed.  Leaving thread.\n");
            break;
        } else {
            pthread_mutex_lock(my->queue->sync_lock);
            my->queue->elem_count -= count;
            pthread_mutex_unlock(my->queue->sync_lock);
        }

        switch(task->type) {
        case SOS_FEEDBACK_TYPE_QUERY: 
            query = (SOSD_query_handle *) task->ref;
            SOS_socket *target = NULL;
            rc = SOS_target_init(SOS, &target,
                    query->reply_host, query->reply_port);
            if (rc != 0) {
                dlog(0, "ERROR: Unable to initialize link to %s:%s"
                        " ... destroying results and returning.\n",
                        query->reply_host,
                        query->reply_port);
                if (query->reply_host != NULL) {
                    free(query->reply_host);
                    query->reply_host = NULL;
                }
                SOSA_results_destroy(query->results);
                free(query);
                free(task);
                continue;
            }

           // printf("SOSD: Sending query results to %s:%d ...\n",
           //         query->reply_host,
           //         query->reply_port);
           // fflush(stdout);

            rc = SOS_target_connect(target);
            if (rc != 0) {
                dlog(0, "Unable to connect to"
                        " client at %s:%d\n",
                        query->reply_host,
                        query->reply_port);
            }

            SOS_buffer_wipe(results_reply_msg);
            SOSA_results_to_buffer(results_reply_msg, query->results);
            SOSA_results_destroy(query->results);

            rc = SOS_target_send_msg(target, results_reply_msg);
            if (rc < 0) {
                dlog(0, "SOSD: Unable to send message to client.\n");
            }
            //printf("SOSD: Done sending message.  (%d bytes sent)\n", rc);
            //fflush(stdout);

            rc = SOS_target_disconnect(target);
            rc = SOS_target_destroy(target);
            if (query->reply_host != NULL) {
                free(query->reply_host);
                query->reply_host = NULL;
            }
           
            //Done delivering query results.
            free(query);
            break;

        case SOS_FEEDBACK_TYPE_PAYLOAD:
            // For non-query payloads we need to assemble a message in the
            // standard format that contains as content the type/size/buffer
            // payload to be extracted inside the client and sent into the
            // callback function.
            
            payload = (SOSD_feedback_payload *) task->ref;
            
            header.msg_size = -1;
            header.msg_type = SOS_FEEDBACK_TYPE_PAYLOAD;
            header.msg_from = SOS->config.comm_rank;
            header.ref_guid = 0;

            SOS_buffer_wipe(delivery);

            dlog(5, "Signing the delivery buffer (initial zip)\n");
            offset = 0;
            SOS_msg_zip(delivery, header, 0, &offset);
            int after_header = offset;

            dlog(5, "Packing the payload data into the delivery buffer.\n");
            // Pack the payload message into the delivery.
            SOS_buffer_pack_bytes(delivery, &offset, payload->size, payload->data);

            dlog(5, "Re-signing it with the known size now (%d)...\n", offset);
            header.msg_size = offset;
            offset = 0;
            SOS_msg_zip(delivery, header, 0, &offset);

            pthread_mutex_lock(SOSD.sync.sense_list_lock);
            SOSD_sensitivity_entry *sense = SOSD.sync.sense_list_head;
            SOSD_sensitivity_entry *prev_sense = NULL;
            while(sense != NULL) {
                if (strcmp(payload->handle, sense->sense_handle) == 0) {
                    rc = SOS_target_connect(sense->target);
                    if (rc < 0) {
                        // Remove this target.
                        fprintf(stderr, "Removing missing target --> %s:%d\n",
                                sense->remote_host,
                                sense->remote_port);
                        fflush(stderr);
                        if (sense == SOSD.sync.sense_list_head) {
                            SOSD.sync.sense_list_head = sense->next_entry;
                        } else {
                            prev_sense->next_entry = sense->next_entry;
                        }
                        free(sense->remote_host);
                        free(sense->sense_handle);
                        SOS_target_destroy(sense->target);
                        sense = NULL;
                        free(sense);
                        sense = prev_sense->next_entry;
                        continue;
                    }

                    /*
                     *  Uncomment in case of emergency:
                     * 
                    fprintf(stderr, "Message for the client at %s:%d  ...\n",
                            sense->remote_host, sense->remote_port);
                    fprintf(stderr, "   header.msg_size == %d\n", header.msg_size);
                    fprintf(stderr, "   header.msg_type == %d\n", header.msg_type);
                    fprintf(stderr, "   header.msg_from == %" SOS_GUID_FMT "\n",
                            header.msg_from);
                    fprintf(stderr, "   header.ref_guid == %" SOS_GUID_FMT "\n",
                            header.ref_guid);
                    fprintf(stderr, "\n");
                    fprintf(stderr, "   offset_after_header == %d\n", after_header);
                    fprintf(stderr, "\n");
                    fprintf(stderr, "   delivery->len == %d\n", delivery->len);
                    fprintf(stderr, "   delivery->max == %d\n", delivery->max);
                    fprintf(stderr, "   delivery->data[offset_after_header"
                            " + 4] == \"%s\"\n",
                            (char *)(delivery->data + after_header + 4));
                    fprintf(stderr, "\n");
                    fprintf(stderr, "   payload->size == %d\n", payload->size);
                    fprintf(stderr, "   payload->data == %s\n", (char *) payload->data);
                    fprintf(stderr, "   ...\n");
                    fprintf(stderr, "\n");
                    fflush(stderr);
                    */
                    SOS_target_send_msg(sense->target, delivery);
                    SOS_target_disconnect(sense->target);
                }
                prev_sense = sense;
                sense = sense->next_entry;
            }

            pthread_mutex_unlock(SOSD.sync.sense_list_lock);

            //Done delivering payload.
            free(payload->data);
            break;

        default:
            dlog(0, "WARNING: Incorrect feedback type specified.  (%d)\n",
                    task->type);

        }

        //Done processing this feedback task.
        dlog(5, "Done processing this feedback task.\n");
        free(task);

        gettimeofday(&now, NULL);
        wait.tv_sec = SOSD_FEEDBACK_SYNC_WAIT_SEC  + (now.tv_sec);
        wait.tv_nsec = SOSD_FEEDBACK_SYNC_WAIT_NSEC
                + (1000 * now.tv_usec);
    }

    dlog(1, "Feedback handler shut down cleanly.\n");
    pthread_mutex_unlock(my->lock);
    pthread_exit(NULL);
}



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
            dlog(6, "Nothing remains in the queue, and the intake"
            " is closed.  Leaving thread.\n");
            break;
        } else {
            pthread_mutex_lock(my->queue->sync_lock);
            my->queue->elem_count -= count;
            pthread_mutex_unlock(my->queue->sync_lock);
        }

        if (buffer == NULL) {
            dlog(6, "   ... *buffer == NULL!\n");
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_LOCAL_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_LOCAL_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        int offset = 0;
        SOS_msg_unzip(buffer, &header, 0, &offset);


        switch(header.msg_type) {
        case SOS_MSG_TYPE_ANNOUNCE:   SOSD_handle_announce   (buffer); break;
        case SOS_MSG_TYPE_PUBLISH:    SOSD_handle_publish    (buffer); break;
        case SOS_MSG_TYPE_VAL_SNAPS:  SOSD_handle_val_snaps  (buffer); break;
        case SOS_MSG_TYPE_KMEAN_DATA: SOSD_handle_kmean_data (buffer); break;
        default:
            dlog(0, "ERROR: An invalid message type (%d) was"
                    " placed in the local_sync queue!\n", header.msg_type);
            dlog(0, "ERROR: Destroying it.\n");
            SOS_buffer_destroy(buffer);
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_LOCAL_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_LOCAL_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        if (SOS->role == SOS_ROLE_LISTENER) {
            pthread_mutex_lock(SOSD.sync.cloud_send.queue->sync_lock);
            pipe_push(SOSD.sync.cloud_send.queue->intake, (void *) &buffer, 1);
            SOSD.sync.cloud_send.queue->elem_count++;
            pthread_mutex_unlock(SOSD.sync.cloud_send.queue->sync_lock);
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
    while (SOS->status == SOS_STATUS_RUNNING
            || my->queue->elem_count > 0)
    {
        pthread_cond_timedwait(my->cond, my->lock, &wait);

        SOSD_countof(thread_db_wakeup++);

        pthread_mutex_lock(my->queue->sync_lock);

        queue_depth = my->queue->elem_count;
        if (queue_depth > 0) {
            task_list = (SOSD_db_task **) calloc(sizeof(SOSD_db_task *),
                    queue_depth);
        } else {
            pthread_mutex_unlock(my->queue->sync_lock);
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_DB_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_DB_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        count = 0;
        count = pipe_pop_eager(my->queue->outlet,
                (void *) task_list, queue_depth);
        if (count == 0) {
            dlog(0, "Nothing remains in the queue and the intake"
                    " is closed.  Leaving thread.\n");
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
                SOSD_db_insert_pub((SOS_pub *) task->ref);
                break;

            case SOS_MSG_TYPE_PUBLISH:
                dlog(6, "Sending PUBLISH to the database...\n");
                SOSD_db_insert_data((SOS_pub *) task->ref);
                break;

            case SOS_MSG_TYPE_VAL_SNAPS:
                dlog(6, "Sending VAL_SNAPS to the database...\n");
                SOSD_db_insert_vals(SOSD.db.snap_queue, NULL);
                break;

            case SOS_MSG_TYPE_QUERY:
                dlog(6, "Sending QUERY to the database...\n");
                SOSD_db_handle_sosa_query((SOSD_db_task *) task);
                break;

            default:
                dlog(0, "WARNING: Invalid task->type value at"
                        " task_list[%d].   (%d)\n",
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

// NOTE: This function is needed for the EVPath implementation.
void* SOSD_THREAD_cloud_recv(void *args) {
    #ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
        SOSD_cloud_listen_loop();
    #endif
    return NULL;
}


void* SOSD_THREAD_cloud_send(void *args) {
    SOSD_sync_context *my = (SOSD_sync_context *) args;
    SOS_SET_CONTEXT(my->sos_context, "SOSD_THREAD_cloud_send");
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

    buffer = NULL;
    reply = NULL;
    SOS_buffer_init_sized_locking(SOS, &buffer,
            (10 * SOS_DEFAULT_BUFFER_MAX), false);
    SOS_buffer_init_sized_locking(SOS, &reply, SOS_DEFAULT_BUFFER_MAX, false);

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
            msg_list = (SOS_buffer **)
                malloc(queue_depth * sizeof(SOS_buffer *));
        } else {
            //Going back to sleep.
            pthread_mutex_unlock(my->queue->sync_lock);
            gettimeofday(&now, NULL);
            wait.tv_sec  = SOSD_CLOUD_SYNC_WAIT_SEC  + (now.tv_sec);
            wait.tv_nsec = SOSD_CLOUD_SYNC_WAIT_NSEC + (1000 * now.tv_usec);
            continue;
        }

        count = 0;
        count = pipe_pop_eager(my->queue->outlet,
                (void *) msg_list, queue_depth);
        if (count == 0) {
            dlog(0, "Nothing in the queue and the intake is closed."
                    "  Leaving thread.\n");
            free(msg_list);
            break;
        }

        my->queue->elem_count -= count;
        pthread_mutex_unlock(my->queue->sync_lock);

        // NOTE: This is an exception to other message packaging formats you see
        //       in SOS, because all daemon<-->daemon messages contain a count
        //       of internal messages as the first value, to be able to buffer
        //       and shuttle groups of messages around rather than incurring
        //       setup and teardown overhead for each relayed message.
        offset = 0;
        SOS_buffer_pack(buffer, &offset, "i", count);

        // Embed all of the messages we've enqueued to send...
        for (msg_index = 0; msg_index < count; msg_index++) {
 
            msg_offset = 0;
            msg = msg_list[msg_index];

            SOS_msg_unzip(msg, &header, 0, &msg_offset);

            while ((header.msg_size + offset) > buffer->max) {
                dlog(1, "(header.msg_size == %d   +   offset == %d)"
                        "  >  buffer->max == %d    (growing...)\n",
                     header.msg_size, offset, buffer->max);
                SOS_buffer_grow(buffer, buffer->max, SOS_WHOAMI);
            }

            dlog(1, "[ccc] (%d of %d) --> packing in "
                    "msg(%15s).size == %d @ offset:%d\n",
                    (msg_index + 1),
                    count,
                    SOS_ENUM_STR(header.msg_type, SOS_MSG_TYPE),
                    header.msg_size,
                    offset);

            memcpy((buffer->data + offset), msg->data, header.msg_size);
            offset += header.msg_size;

            SOS_buffer_destroy(msg);
        }

        buffer->len = offset;
        dlog(0, "[ccc] Sending %d messages in %d bytes"
                " to aggregator ...\n", count, buffer->len);

        SOSD_cloud_send(buffer, reply);

        //TODO: Handle replies here (MPI only...?)

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



void
SOSD_send_to_self(SOS_buffer *send_buffer, SOS_buffer *reply_buffer) {
    SOS_SET_CONTEXT(send_buffer->sos_context, "SOSD_send_to_self");

    dlog(1, "Preparing to send a message to our own listening socket...\n");
    dlog(1, "  ... Initializing...\n");
    SOS_socket *my_own_listen_port = NULL;
    SOS_target_init(SOS, &my_own_listen_port, "localhost", 
            atoi(getenv("SOS_CMD_PORT")));
    dlog(1, "  ... Connecting...\n");
    SOS_target_connect(my_own_listen_port);
    dlog(1, "  ... Sending...\n");
    SOS_target_send_msg(my_own_listen_port, send_buffer);
    dlog(1, "  ... Receiving reply...\n");
    SOS_target_recv_msg(my_own_listen_port, reply_buffer);
    dlog(1, "  ... Disconnecting...\n");
    SOS_target_disconnect(my_own_listen_port);

    dlog(1, "Done.\n");

    return;
}


void
SOSD_handle_desensitize(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_");
    
    SOS_msg_header header;
    int rc;
    
    // 1. Remove the specific sensitivity GUID+handle combo
    
    SOS_buffer *reply = NULL;
    SOS_buffer_init_sized_locking(SOS, &reply, 64, false);
    SOSD_PACK_ACK(reply);

    rc = SOS_target_send_msg(SOSD.net, reply);
    
    dlog(5, "Replying with reply->len == %d bytes, rc == %d\n",
            reply->len, rc);
    if (rc == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        SOSD_countof(socket_bytes_sent += rc);
    }
    SOS_buffer_destroy(reply);
    dlog(5, "Done.\n");
    return;
}


void
SOSD_handle_unregister(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_");
    
    SOS_msg_header header;
    int rc;
    
    // -- This message is sent when a client calls SOS_finalize();
    // TODO: Verify that all process-related tasks are concluded here
    // 1. Stop collecting PID information
    // 2. Inject a timestamp in the database
    // 3. Destroy all pubs that belong to this GUID
    // 4. Remove all sensitivities for this GUID
    
    SOS_buffer *reply = NULL;
    SOS_buffer_init_sized_locking(SOS, &reply, 64, false);
    SOSD_PACK_ACK(reply);
    
    rc = SOS_target_send_msg(SOSD.net, reply);

    dlog(5, "Replying with reply->len == %d bytes, rc == %d\n",
            reply->len, rc);
    if (rc == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        SOSD_countof(socket_bytes_sent += rc);
    }
    SOS_buffer_destroy(reply);
    dlog(5, "Done.\n");
    return;
}



void
SOSD_handle_sensitivity(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_sensitivity");

    dlog(5, "Registering a client sensitivity.\n");
    
    SOS_msg_header header;
    
    int offset = 0;
    SOS_msg_unzip(msg, &header, 0, &offset);
   
    SOSD_sensitivity_entry *sense =
        calloc(1, sizeof(SOSD_sensitivity_entry));

    sense->sense_handle = NULL;
    sense->client_guid = header.msg_from;
    sense->remote_host = NULL;
    sense->remote_port = -1;
    
    SOS_buffer_unpack_safestr(msg, &offset, &sense->sense_handle);
    SOS_buffer_unpack_safestr(msg, &offset, &sense->remote_host);
    SOS_buffer_unpack(msg, &offset, "i", &sense->remote_port);

    bool OK_to_add = true;

    pthread_mutex_lock(SOSD.sync.sense_list_lock);

    SOSD_sensitivity_entry *entry = SOSD.sync.sense_list_head;
    while (entry != NULL) {
        if ((entry->client_guid == header.msg_from)
         && (strcmp(sense->sense_handle, entry->sense_handle) == 0))
        {
            // This entry is effectively a duplicate... do nothing.
            entry = NULL;
            OK_to_add = false;
        } else {
            entry = entry->next_entry;
        }
    }

    if (OK_to_add) {
        sense->guid = SOS_uid_next(SOSD.guid);
        sense->next_entry = SOSD.sync.sense_list_head;
        sense->target = NULL;
        SOS_target_init(SOS, &sense->target,
                sense->remote_host, sense->remote_port);
        SOSD.sync.sense_list_head = sense;
    } else {
        free(sense->sense_handle);
        free(sense->remote_host);
        free(sense);
    }

    pthread_mutex_unlock(SOSD.sync.sense_list_lock);


    SOS_buffer *reply = NULL;
    SOS_buffer_init_sized_locking(SOS, &reply, 256, false);
    SOSD_PACK_ACK(reply);
    int rc = 0;
    SOS_target_send_msg(SOSD.net, reply);
    dlog(5, "replying with reply->len == %d bytes, rc == %d\n",
            reply->len, rc);
    if (rc == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        SOSD_countof(socket_bytes_sent += rc);
    }
    SOS_buffer_destroy(reply);
    dlog(5, "Done.\n");
    return;
}


void
SOSD_handle_triggerpull(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_triggerpull");

    dlog(5, "Trigger pull message received.  Passing to cloud functions.\n");
    #ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
    SOSD_evpath_handle_triggerpull(msg);
    #else
    fprintf(stderr, "WARNING: Trigger message received, but support for\n"
            "         handling this message has not been compiled into this build\n"
            "         of SOSflow.  Sending an ACK and doing nothing.\n");
    #endif

    dlog(5, "Cloud function returned, sending ACK reply to client"
            "that pulled the trigger.\n");
    SOS_buffer *reply;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);
    SOSD_PACK_ACK(reply);

    int rc;
    rc = SOS_target_send_msg(SOSD.net, reply);

    if (rc == -1) {
            dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", rc);
        SOSD_countof(socket_bytes_sent += rc);
    }

    SOS_buffer_destroy(reply);

    return;
}


void SOSD_handle_kmean_data(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_kmean_data");
    // For parsing the buffer:
    char            pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    SOS_pub         *pub;
    SOS_msg_header   header;
    int              offset;
    int              rc;
    // For unpacking / using the contents:
    SOS_guid         guid;
    SOSD_km2d_point  point;
        
    dlog(5, "header.msg_type = SOS_MSG_TYPE_KMEAN_DATA\n");

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT,
            header.ref_guid);

    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table,pub_guid_str);

    if (pub == NULL) {
        dlog(0, "ERROR: No pub exists for header.ref_guid"
                " == %" SOS_GUID_FMT "\n", header.ref_guid); 
        dlog(0, "ERROR: Destroying message and returning.\n");
        SOS_buffer_destroy(buffer);
        return;
    }

    dlog(5, "Pub handle found for this KMEANS2D data.\n");

    //TODO: kmean2d -- process data for this pub handle
    //      this function is present a stub, but the protocol
    //      is in pleace cleanly now


    dlog(5, "Nothing remains to do, returning.\n");
    return;
}



void SOSD_handle_query(SOS_buffer *buffer) { 
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_sosa_query");
    SOS_msg_header header;
    int            offset;
    int            rc;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_QUERY\n");

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);
    
    SOSD_query_handle *query_handle = NULL;
    query_handle = (SOSD_query_handle *)
            calloc(1, sizeof(SOSD_query_handle));

    query_handle->state          = SOS_QUERY_STATE_INCOMING;
    query_handle->reply_to_guid  = header.msg_from;
    query_handle->query_guid     = -1;
    query_handle->query_sql      = NULL;
    query_handle->reply_host     = NULL;
    query_handle->reply_port     = -1;

    dlog(6, "   ...extracting query...\n");
    SOS_buffer_unpack_safestr(buffer, &offset, &query_handle->reply_host);
    SOS_buffer_unpack(buffer, &offset, "i", &query_handle->reply_port);
    SOS_buffer_unpack_safestr(buffer, &offset, &query_handle->query_sql);
    SOS_buffer_unpack(buffer, &offset, "g", &query_handle->query_guid);

    dlog(6, "      ...received reply_host: \"%s\"\n",
            query_handle->reply_host);
    dlog(6, "      ...received reply_port: \"%d\"\n",
            query_handle->reply_port);
    dlog(6, "      ...received query_sql: \"%s\"\n",
            query_handle->query_sql);

    dlog(6, "   ...placing query in db queue.\n");
    SOSD_db_task *task = NULL;
    task = (SOSD_db_task *) calloc(1, sizeof(SOSD_db_task));
    task->ref = (void *) query_handle;
    task->type = SOS_MSG_TYPE_QUERY;
    pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
    pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
    SOSD.sync.db.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);

    dlog(6, "   ...sending ACK to client.\n");
    SOS_buffer *reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);
    SOSD_PACK_ACK(reply);
    SOS_target_send_msg(SOSD.net, reply);
    dlog(5, "replying with reply->len == %d bytes, rc == %d\n",
            reply->len, rc);
    if (rc == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        SOSD_countof(socket_bytes_sent += rc);
    }
    SOS_buffer_destroy(reply);
    dlog(6, "Done.\n");
    return;
}



void SOSD_handle_echo(SOS_buffer *buffer) { 
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_echo");
    int            rc;
    dlog(5, "header.msg_type = SOS_MSG_TYPE_ECHO\n");
    
    SOS_msg_header header;
    
    int offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

    dlog(5, "Message unzipped.  Sending it back to the sender.\n");

    rc = send(SOSD.net->remote_socket_fd, (void *) buffer->data,
            buffer->len, 0);
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
    char          pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int            offset;
    int            rc;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_VAL_SNAPS\n");

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);
    
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT,
            header.ref_guid);

    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table,pub_guid_str);

    if (pub == NULL) {
        dlog(0, "ERROR: No pub exists for header.ref_guid"
                " == %" SOS_GUID_FMT "\n", header.ref_guid); 
        dlog(0, "ERROR: Destroying message and returning.\n");
        SOS_buffer_destroy(buffer);
        return;
    }

    dlog(5, "Injecting snaps into SOSD.db.snap_queue...\n");
    SOS_val_snap_queue_from_buffer(buffer, SOSD.db.snap_queue, pub);


    dlog(5, "Queue these val snaps up for the database...\n");
    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->ref = (void *) pub;
    task->type = SOS_MSG_TYPE_VAL_SNAPS;

    pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
    pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
    SOSD.sync.db.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);
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

        SOS_msg_zip(copy, header, 0, &offset);

        // Enqueue it to be handled:
        pthread_mutex_lock(SOSD.sync.local.queue->sync_lock);
        pipe_push(SOSD.sync.local.queue->intake, (void *)&copy, 1);
        SOSD.sync.local.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.local.queue->sync_lock);
    }

    return;
}



void SOSD_handle_register(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_register");
    SOS_msg_header header;
    SOS_buffer    *reply;
    int            offset;
    int            i;
    SOS_guid       guid_block_from;
    SOS_guid       guid_block_to;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_REGISTER\n");

    reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_BUFFER_MAX);


    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

    //Check version of the client against the server's:
    int client_version_major = -1;
    int client_version_minor = -1;

    SOS_buffer_unpack(buffer, &offset, "ii",
            &client_version_major,
            &client_version_minor);

    if ((client_version_major != SOS_VERSION_MAJOR)
        || (client_version_minor != SOS_VERSION_MINOR)) {
        fprintf(stderr, "CRITICAL WARNING: SOS client library (%d.%d) and"
                " daemon (%d.%d) versions differ!\n",
                client_version_major,
                client_version_minor,
                SOS_VERSION_MAJOR,
                SOS_VERSION_MINOR);
        fprintf(stderr, "                  ** DAEMON ** Attempting"
                " service anyway...\n");
        fflush(stderr);
    }

    if (header.msg_from == 0) {
        /* A new client is registering with the daemon.
         * Supply them a block of GUIDs ...
         */
        SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK,
                &guid_block_from, &guid_block_to);

        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = SOS->config.comm_rank;
        header.ref_guid = 0;

        offset = 0;
        SOS_msg_zip(reply, header, 0, &offset);
        dlog(0, "REGISTER: Providing client with GUIDs _from=%" SOS_GUID_FMT
                ", _to=%" SOS_GUID_FMT "\n", guid_block_from, guid_block_to);
        //Pack in the GUID's
        SOS_buffer_pack(reply, &offset, "gg",
                guid_block_from,
                guid_block_to);

        //Pack in the server's version # (just in case)
        SOS_buffer_pack(reply, &offset, "ii",
                SOS_VERSION_MAJOR,
                SOS_VERSION_MINOR);

        header.msg_size = offset;
        offset = 0;
        SOS_msg_zip(reply, header, 0, &offset);
    } else {
        /* An existing client (such as a scripting language wrapped library)
         * is coming back online, so don't give them any GUIDs.
         */
        SOSD_PACK_ACK(reply);
    }

    i = send( SOSD.net->remote_socket_fd, (void *) reply->data, reply->len, 0 );

    if (i == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
        SOSD_countof(socket_bytes_sent += i);
    }

    SOS_buffer_destroy(reply);

    //TODO: Inject a timestamp in the database for this client.
    //(Pub handle has it?)

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

    reply = NULL;
    SOS_buffer_init_sized_locking(SOS, &reply, SOS_DEFAULT_BUFFER_MAX, false);

    SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK,
            &block_from, &block_to);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;

    offset = 0;
    SOS_msg_zip(reply, header, 0, &offset);

    SOS_buffer_pack(reply, &offset, "gg",
            block_from,
            block_to);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(reply, header, 0, &offset);

    i = send( SOSD.net->remote_socket_fd, (void *) reply->data, reply->len, 0 );

    if (i == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    }
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
    
    char           pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int             offset;
    int             i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_ANNOUNCE\n");

    reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);
    
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%"
            SOS_GUID_FMT, header.ref_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table,pub_guid_str);

    bool firstAnnouncement = false;

    if (pub == NULL) {
        dlog(5, "     ... NOPE!  Adding new pub to the table.\n");
        /* If it's not in the table, add it. */
        SOS_pub_create(SOS, &pub,pub_guid_str, SOS_NATURE_DEFAULT);
        SOSD_countof(pub_handles++);
        strncpy(pub->guid_str,pub_guid_str, SOS_DEFAULT_STRING_LEN);
        pub->guid = header.ref_guid;
        SOSD.pub_table->put(SOSD.pub_table,pub_guid_str, pub);
        // Make sure the pub only goes in once...
        firstAnnouncement = true;
    } else {
        dlog(5, "     ... FOUND IT!\n");
        firstAnnouncement = false;
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n",
            SOSD.pub_table->size(SOSD.pub_table));
    dlog(5, "Calling SOSD_apply_announce() ...\n");

    SOSD_apply_announce(pub, buffer);
    pub->announced = SOSD_PUB_ANN_DIRTY;
   
    if (firstAnnouncement == true) {
        task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
        task->ref = (void *) pub;
        task->type = SOS_MSG_TYPE_ANNOUNCE;
        pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
        pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
        SOSD.sync.db.queue->elem_count++;
        SOS_buffer_destroy(reply);
        pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);
    }

    dlog(5, "  ... pub(%" SOS_GUID_FMT ")->elem_count = %d\n",
            pub->guid, pub->elem_count);

    return;
}


void SOSD_handle_publish(SOS_buffer *buffer)  {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_publish");
    SOSD_db_task   *task;
    SOS_msg_header  header;
    SOS_buffer     *reply;
    SOS_pub        *pub;
    char           pub_guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    int             offset;
    int             i;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_PUBLISH\n");

    reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);
    
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT,
            header.ref_guid);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table,pub_guid_str);

    // Check the table for this pub ...
    dlog(5, "  ... checking SOS->pub_table for GUID(%s):\n",pub_guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table,pub_guid_str);

    if (pub == NULL) {
        // If it's not in the table, add it.
        dlog(0, "ERROR: PUBLISHING INTO A PUB (guid:%" SOS_GUID_FMT
            ") NOT FOUND! (WEIRD!)\n", header.ref_guid);
        dlog(0, "ERROR: .... ADDING previously unknown pub to the table..."
            " (this is bogus, man)\n");
        SOS_pub_create(SOS, &pub,pub_guid_str, SOS_NATURE_DEFAULT);
        SOSD_countof(pub_handles++);
        strncpy(pub->guid_str,pub_guid_str, SOS_DEFAULT_STRING_LEN);
        pub->guid = header.ref_guid;
        SOSD.pub_table->put(SOSD.pub_table,pub_guid_str, pub);
    } else {
        dlog(5, "     ... FOUND it!\n");
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n",
            SOSD.pub_table->size(SOSD.pub_table));

    SOSD_apply_publish(pub, buffer);

    task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
    task->ref = (void *) pub;
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
        SOS_msg_zip(copy, header, 0, &offset);
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

    if (SOS->role == SOS_ROLE_AGGREGATOR) {
        fprintf(stderr, "WARNING: Attempt to trigger shutdown"
                " at AGGREGATOR.\n");
        dlog(0, "CRITICAL WARNING: Shutdown was triggered on an AGGREGATION\n"
                "    daemon. Shutdown should be sent to each in situ LISTENER,\n"
                "    and they will automatically propagate the notification via\n"
                "    inter-daemon communication channels to the appropriate\n"
                "    aggregation targets, if any exist.\n"
                "    ... attempting to proceed anyway.\n");
    }

    reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

    if (SOS->role == SOS_ROLE_LISTENER) {
        dlog(5, "Replying to the client who submitted the shutdown"
                " request in situ...\n");
        SOSD_PACK_ACK(reply);
        i = send(SOSD.net->remote_socket_fd, (void *) reply->data,
                reply->len, 0);
        if (i == -1) {
            dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
        } else {
            dlog(5, "  ... send() returned the following bytecount: %d\n", i);
            SOSD_countof(socket_bytes_sent += i);
        }
    }

    #if (SOSD_CLOUD_SYNC > 0)
    SOSD_cloud_shutdown_notice();
    #endif

    SOSD.daemon.running = 0;
    SOS->status = SOS_STATUS_SHUTDOWN;

    SOS_buffer_destroy(reply);

    dlog(5, "Done.\n");

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

    reply = NULL;
    SOS_buffer_init(SOS, &reply);

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

    if (SOS->role == SOS_ROLE_LISTENER) {
        // Build a reply:
        memset(&header, '\0', sizeof(SOS_msg_header));
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_FEEDBACK;
        header.msg_from = 0;
        header.ref_guid = 0;

        offset = 0;
        SOS_msg_zip(reply, header, 0, &offset);

        // TODO: { FEEDBACK } Currently this is a hard-coded 'exec function' case.
        // This needs to get turned into a 'mailbox' kind of system.
        snprintf(function_name, SOS_DEFAULT_STRING_LEN, "demo_function");

        SOS_buffer_pack(reply, &offset, "is",
            SOS_FEEDBACK_TYPE_PAYLOAD,
            function_name);

        header.msg_size = offset;
        offset = 0;
        SOS_msg_zip(reply, header, 0, &offset);


        dlog(1, "Replying to CHECK_IN with SOS_FEEDBACK_EXEC_FUNCTION(%s)"
                "...\n", function_name);

        i = send( SOSD.net->remote_socket_fd, (void *) reply->data,
                reply->len, 0 );
        if (i == -1) {
            dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
        } else {
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

    reply = NULL;
    SOS_buffer_init_sized_locking(SOS, &reply, SOS_DEFAULT_BUFFER_MAX, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_PROBE;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = -1;

    dlog(5, "Assembling probe data structure...\n");

    int offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

    uint64_t queue_depth_local     = SOSD.sync.local.queue->elem_count;
    uint64_t queue_depth_cloud     = 0;
    if (SOS->role == SOS_ROLE_LISTENER) {
        queue_depth_cloud          = SOSD.sync.cloud_send.queue->elem_count;
    }
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
        dlog(0, "ERROR: Could not open %s file for probe request.   (%s)\n",
                mf_path, strerror(errno));
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

    dlog(5, "   ...sending probe results, len = %d\n", reply->len);
    i = send( SOSD.net->remote_socket_fd, (void *) reply->data,
            reply->len, 0 );
    if (i == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
        SOSD_countof(socket_bytes_sent += i);
    }

    SOS_buffer_destroy(reply);
    dlog(5, "   ...done.\n");
    return;
}



void SOSD_handle_unknown(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_unknown");
    SOS_msg_header  header;
    SOS_buffer     *reply;
    int             offset;
    int             i;

    dlog(1, "header.msg_type = UNKNOWN\n");

    reply = NULL;
    SOS_buffer_init(SOS, &reply);

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

    dlog(1, "header.msg_size == %d\n", header.msg_size);
    dlog(1, "header.msg_type == %d\n", header.msg_type);
    dlog(1, "header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(1, "header.ref_guid == %" SOS_GUID_FMT "\n", header.ref_guid);

    if (SOS->role == SOS_ROLE_AGGREGATOR) {
        SOS_buffer_destroy(reply);
        return;
    }

    SOSD_PACK_ACK(reply);

    i = send(SOSD.net->remote_socket_fd, (void *) reply->data,
            reply->len, 0);
    if (i == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
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

    memset(&SOSD.net->local_hint, '\0', sizeof(struct addrinfo));
    SOSD.net->local_hint.ai_family     = AF_UNSPEC;     // Allow IPv4 or IPv6
    SOSD.net->local_hint.ai_socktype   = SOCK_STREAM;   // STREAM/DGRAM/RAW
    SOSD.net->local_hint.ai_flags      = AI_PASSIVE;
    SOSD.net->local_hint.ai_protocol   = 0;
    SOSD.net->local_hint.ai_canonname  = NULL;
    SOSD.net->local_hint.ai_addr       = NULL;
    SOSD.net->local_hint.ai_next       = NULL;

    i = getaddrinfo(NULL, SOSD.net->local_port, &SOSD.net->local_hint,
            &SOSD.net->result_list);
    if (i != 0) {
       dlog(0, "Error!  getaddrinfo() failed. (%s)"
            " Exiting daemon.\n", gai_strerror(errno));
       exit(EXIT_FAILURE);
    }

    for ( SOSD.net->local_addr = SOSD.net->result_list ;
            SOSD.net->local_addr != NULL ;
            SOSD.net->local_addr = SOSD.net->local_addr->ai_next )
    {
        dlog(1, "Trying an address...\n");

        SOSD.net->local_socket_fd =
            socket(SOSD.net->local_addr->ai_family,
                    SOSD.net->local_addr->ai_socktype,
                    SOSD.net->local_addr->ai_protocol);
        if ( SOSD.net->local_socket_fd < 1) {
            dlog(0, "  ... failed to get a socket.  (%s)\n", strerror(errno));
            continue;
        }

         // Allow this socket to be reused/rebound quickly by the daemon.
        if ( setsockopt( SOSD.net->local_socket_fd, SOL_SOCKET,
                    SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            dlog(0, "  ... could not set socket options.  (%s)\n", 
                    strerror(errno));
            continue;
        }

        if ( bind(SOSD.net->local_socket_fd,
                    SOSD.net->local_addr->ai_addr,
                    SOSD.net->local_addr->ai_addrlen) == -1 )
        {
            dlog(0, "  ... failed to bind to socket.  (%s)\n",
                    strerror(errno));
            close( SOSD.net->local_socket_fd );
            continue;
        } 
        // If we get here, we're good to stop looking.
        break;
    }

    if ( SOSD.net->local_socket_fd < 0 ) {
        dlog(0, "  ... could not socket/setsockopt/bind to anything in the"
                " result set.  last errno = (%d:%s)\n",
                errno, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... got a socket, and bound to it!\n");
    }

    freeaddrinfo(SOSD.net->result_list);

    // Enforce that this is a BLOCKING socket:
    opts = fcntl(SOSD.net->local_socket_fd, F_GETFL);
    if (opts < 0) {
        dlog(0, "ERROR!  Cannot call fcntl() on the"
                " local_socket_fd to get its options.  Carrying on.  (%s)\n",
                strerror(errno));
    }
 
    opts = opts & !(O_NONBLOCK);
    i    = fcntl(SOSD.net->local_socket_fd, F_SETFL, opts);
    if (i < 0) {
        dlog(0, "ERROR!  Cannot use fcntl() to set the"
                " local_socket_fd to BLOCKING more.  Carrying on.  (%s).\n",
                strerror(errno));
    }


    listen( SOSD.net->local_socket_fd, SOSD.net->listen_backlog );
    dlog(0, "Listening on socket.\n");

    return;
}
 


void SOSD_init() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_init");
    pid_t pid, ppid, sid;
    int rc;

    // [daemon name]
    switch (SOS->role) {
    case SOS_ROLE_LISTENER:
        snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN,
                "%s", SOSD_DAEMON_NAME /* ".mon" */);
        break;
    case SOS_ROLE_AGGREGATOR:
        snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN,
                "%s", SOSD_DAEMON_NAME /* ".dat" */);
        break;
    default: break;
    }

    // [lock file]
    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.daemon.lock_file, SOS_DEFAULT_STRING_LEN,
            "%s/%s.%05d.lock", SOSD.daemon.work_dir,
            SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.daemon.lock_file, SOS_DEFAULT_STRING_LEN,
            "%s/%s.local.lock", SOSD.daemon.work_dir, SOSD.daemon.name);
    #endif
    sos_daemon_lock_fptr = open(SOSD.daemon.lock_file, O_RDWR | O_CREAT, 0640);
    if (sos_daemon_lock_fptr < 0) {
        fprintf(stderr, "\nERROR!  Unable to start daemon (%s):"
                " Could not access lock file %s in directory %s\n",
                SOSD.daemon.name,
                SOSD.daemon.lock_file,
                SOSD.daemon.work_dir);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    if (lockf(sos_daemon_lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\nERROR!  Unable to start daemon (%s):"
                " Could not lock instance id file %s in directory %s\n",
                SOSD.daemon.name,
                SOSD.daemon.lock_file,
                SOSD.daemon.work_dir);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) {
        printf("Lock file obtained.  (%s)\n", SOSD.daemon.lock_file);
        fflush(stdout);
    }


    // [log file]
#if (SOSD_DAEMON_LOG > 0)
    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN,
            "%s/%s.%05d.log", SOSD.daemon.work_dir,
            SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN,
            "%s/%s.local.log", SOSD.daemon.work_dir, SOSD.daemon.name);
    #endif
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { 
        printf("Opening log file: %s\n", SOSD.daemon.log_file);
        fflush(stdout);
    }
    sos_daemon_log_fptr = fopen(SOSD.daemon.log_file, "w"); 
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) {
        printf("  ... done.\n"); fflush(stdout);
    }
#endif


    if (!SOSD_ECHO_TO_STDOUT) {
        dlog(1, "Logging output up to this point has been suppressed, but"
                " all initialization has gone well.\n");
        dlog(1, "Log file is now open.  Proceeding...\n");
        dlog(1, "SOSD_init():\n");
    }

    // [mode]
    #if (SOSD_DAEMON_MODE > 0)
    {
    dlog(1, "  ...mode: DETACHED DAEMON (fork/umask/sedsid)\n");
        // [fork]
        ppid = getpid();
        pid  = fork();
        
        if (pid < 0) {
            dlog(0, "ERROR! Unable to start daemon (%s): Could not fork()"
                    " off parent process.\n", SOSD.daemon.name);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent
        
        // [child session]
        umask(0);
        sid = setsid();
        if (sid < 0) {
            dlog(0, "ERROR!  Unable to start daemon (%s): Could not acquire"
                    " a session id.\n", SOSD_DAEMON_NAME); 
            exit(EXIT_FAILURE);
        }
        if ((chdir(SOSD.daemon.work_dir)) < 0) {
            dlog(0, "ERROR!  Unable to start daemon (%s): Could not change"
                    " to working directory: %s\n", SOSD_DAEMON_NAME,
                    SOSD.daemon.work_dir);
            exit(EXIT_FAILURE);
        }
        
        dlog(1, "  ... session(%d) successfully split off from parent(%d).\n",
                getpid(), ppid);
    }
    #else
    {
        dlog(1, "  ... mode: ATTACHED INTERACTIVE\n");
    }
    #endif

    sprintf(SOSD.daemon.pid_str, "%d", getpid());
    dlog(1, "  ... pid: %s\n", SOSD.daemon.pid_str);

    // Now we can write our PID out to the lock file safely...
    rc = write(sos_daemon_lock_fptr, SOSD.daemon.pid_str,
            strlen(SOSD.daemon.pid_str));



    // [guid's]
    dlog(1, "Obtaining this instance's guid range...\n");
    #if (SOSD_CLOUD_SYNC > 0)
        SOS_guid guid_block_size = (SOS_guid)
            (SOS_DEFAULT_UID_MAX / (SOS_guid) SOS->config.comm_size);
        SOS_guid guid_my_first   = (SOS_guid)
            SOS->config.comm_rank * guid_block_size;
        SOS_uid_init(SOS, &SOSD.guid, guid_my_first,
                (guid_my_first + (guid_block_size - 1)));
    #else
        dlog(1, "DATA NOTE:  Running in local mode,"
                " CLOUD_SYNC is disabled.\n");
        dlog(1, "DATA NOTE:  GUID values are unique only to this node.\n");
        SOS_uid_init(&SOSD.guid, 1, SOS_DEFAULT_UID_MAX);
    #endif
    // Set up the GUID pool for daemon-internal pub handles.
    SOSD.sos_context->uid.my_guid_pool = SOSD.guid;
    dlog(1, "  ... (%" SOS_GUID_FMT " ---> %" SOS_GUID_FMT ")\n",
            SOSD.guid->next, SOSD.guid->last);

    // [hashtable]
    dlog(1, "Setting up a hash table for pubs...\n");
    SOSD.pub_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(1, "Daemon initialization is complete.\n");
    SOSD.daemon.running = 1;
    return;
}



void
SOSD_sync_context_init(
        SOS_runtime *sos_context, 
        SOSD_sync_context *sync_context,
        size_t elem_size, void* (*thread_func)(void *thread_param))
{
    SOS_SET_CONTEXT(sos_context, "SOSD_sync_context_init");

    sync_context->sos_context = sos_context;
    if (elem_size > 0) {
        SOS_pipe_init(SOS, &sync_context->queue, elem_size);
    }
    sync_context->handler =
        (pthread_t *) malloc(sizeof(pthread_t));
    sync_context->lock    =
        (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    sync_context->cond    =
        (pthread_cond_t *) malloc(sizeof(pthread_cond_t));

    pthread_mutex_init(sync_context->lock, NULL);
    pthread_cond_init(sync_context->cond, NULL);
    pthread_create(sync_context->handler, NULL, thread_func,
            (void *) sync_context);

    return;
}

void
SOSD_claim_guid_block(
        SOS_uid *id,
        int size,
        SOS_guid *pool_from,
        SOS_guid *pool_to)
{
    SOS_SET_CONTEXT(id->sos_context, "SOSD_claim_guid_block");

    pthread_mutex_lock( id->lock );

    if ((id->next + size) > id->last) {
        // This is basically a failure case if any more GUIDs are requested. 
        *pool_from = id->next;
        *pool_to   = id->last;
        id->next   = id->last + 1;
    } else {
        *pool_from = id->next;
        *pool_to   = id->next + size;
        id->next   = id->next + size + 1;
        dlog(0, "served GUID block: %" SOS_GUID_FMT " ----> %"
                SOS_GUID_FMT "\n",
                *pool_from, *pool_to);
    }

    pthread_mutex_unlock( id->lock );

    return;
}


void SOSD_apply_announce( SOS_pub *pub, SOS_buffer *buffer ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_announce");

    dlog(6, "Calling SOS_announce_from_buffer()...\n");
    SOS_announce_from_buffer(buffer, pub);
    SOSD_add_pid_to_track(pub);

    return;
}


void SOSD_apply_publish( SOS_pub *pub, SOS_buffer *buffer ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_publish");

    dlog(6, "Calling SOS_publish_from_buffer()...\n");
    SOS_publish_from_buffer(buffer, pub, SOSD.db.snap_queue);

    return;
}





void SOSD_display_logo(void) {

    int col = 0;
    int choice = 0;
    srand(getpid());
    choice = rand() % 3;

    if (getenv("SOS_BATCH_ENVIRONMENT") != NULL) {
        // Don't print colors or extended characters...
        printf("========================================="
               "=========================================\n");
        printf("-----------------------------------------"
               "-----------------------------------------\n");
        printf("\n");

    } else {
        printf("\n");
        printf(SOS_BOLD_WHT);
        for (col = 0; col < 79; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR SOS_WHT);
        for (col = 0; col < 1; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR "\n" SOS_WHT);
        for (col = 0; col < 1; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR SOS_DIM_WHT);
        for (col = 0; col < 79; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR "\n\n");
    
        printf(SOS_BOLD_GRN);
    }

    switch (choice) {
    case 0:
        printf("           _/_/_/    _/_/      _/_/_/ "
                "||[]||[]}))))}]|[]||||  Scalable\n");
        printf("        _/        _/    _/  _/        "
                "|||||[][{(((({[]|[]|||  Observation\n");
        printf("         _/_/    _/    _/    _/_/     "
                "|[][]|[]}))))}][]|||[]  System\n");
        printf("            _/  _/    _/        _/    "
                "[]|||[][{(((({[]|[]|||  for Scientific\n");
        printf("     _/_/_/      _/_/    _/_/_/       "
                "||[]||[]}))))}][]||[]|  Workflows\n");
        break;


    case 1:
        printf("             .____________________________________"
                "_________________ ___  _ _\n");
        printf("            /_____/\\/\\/\\/\\/\\____/\\/\\/\\/\\_"
                "_____/\\/\\/\\/\\/\\________ ___ _ _\n");
        printf("           /___/\\/\\__________/\\/\\____/\\/\\__/"
                "\\/\\_______________ ___  _   __ _\n");
        printf("          /_____/\\/\\/\\/\\____/\\/\\____/\\/\\__"
                "__/\\/\\/\\/\\_________  _ __\n");
        printf("         /___________/\\/\\__/\\/\\____/\\/\\_____"
                "_____/\\/\\_______ ___ _  __ _   _\n");
        printf("        /___/\\/\\/\\/\\/\\______/\\/\\/\\/\\____/"
                "\\/\\/\\/\\/\\_________ ___ __ _  _   _\n");
        printf("       /__________________________________________"
                "___________ _  _      _\n");
        printf("      /___/\\/\\/\\__/\\/\\_______________________"
                "____________ ___ _ __ __  _    _\n");
        printf("     /___/\\/\\______/\\/\\______/\\/\\/\\____/\\/"
                "\\______/\\/\\___ ___ _ ___ __ _  _\n");
        printf("    /___/\\/\\/\\____/\\/\\____/\\/\\__/\\/\\__/\\/"
                "\\__/\\__/\\/\\___ __ _ _  _  _  _\n");
        printf("   /___/\\/\\______/\\/\\____/\\/\\__/\\/\\__/\\/\\/"
                "\\/\\/\\/\\/\\____ __ ___  _\n");
        printf("  /___/\\/\\______/\\/\\/\\____/\\/\\/\\______/\\/"
                "\\__/\\/\\_____ ___     _\n");
        printf(" |________________________________________________"
                "__ ___ _  _ _  ___  _\n");
        printf("\n");
        printf("         Scalable Observation System for "
                "Scientific Workflows        \n");
        break;


    case 2:
        printf("_._____. .____  .____.._____. _  ..\n");
        printf("__  ___/_  __ \\_  ___/__  __/__  /__"
                "______      __    .:|   Scalable\n");
        printf(".____ \\_  / / /.___ \\__  /_ ._  /_ "
                " __ \\_ | /| / /    .:|   Observation\n");
        printf("____/ // /_/ /.___/ /_  __/ _  / / /_"
                "/ /_ |/ |/ /     .:|   System for "
                "Scientific\n");
        printf("/____/ \\____/ /____/ /_/    /_/  \\_"
                "___/____/|__/      .:|   Workflows\n");
        break;

    case 99:
        printf("             _/_/_/    _/_/      _/_/_/\n");
        printf("          _/        _/    _/  _/       \n");
        printf("           _/_/    _/    _/    _/_/    \n");
        printf("              _/  _/    _/        _/   \n");
        printf("       _/_/_/      _/_/    _/_/_/      \n");
        printf("\n");
        printf("    ||[]||[]}))))}]|[]|||| Scalable\n");
        printf("    |||||[][{(((({[]|[]||| Observation\n");               
        printf("    |[][]|[]}))))}][]|||[] System\n");
        printf("    []|||[][{(((({[]|[]||| for Scientific\n");  
        printf("    ||[]||[]}))))}][]||[]| Workflows\n");      
        break;

    }

    
    if (getenv("SOS_BATCH_ENVIRONMENT") == NULL) {
        printf(SOS_CLR);
        printf(SOS_GRN);

        printf("\n");
        printf(SOS_GRN "             Version: " SOS_CLR "%d.%d\n",
                SOS_VERSION_MAJOR,
                SOS_VERSION_MINOR);
        printf(SOS_GRN "      Target machine: " SOS_CLR "%s\n",
                SOS_QUOTE_DEF_STR(SOS_HOST_KNOWN_AS));
        printf("\n");
        printf(SOS_GRN "         Compiled on: " SOS_CLR "%s" SOS_GRN " at "
                SOS_CLR "%s" SOS_GRN " by " SOS_CLR "%s@%s\n",
                __DATE__, __TIME__,
                SOS_QUOTE_DEF_STR(SOS_BUILDER),
                SOS_QUOTE_DEF_STR(SOS_HOST_NODE_NAME));
        printf(SOS_GRN "   Build environment: " SOS_CLR "%s\n",
                SOS_QUOTE_DEF_STR(SOS_HOST_DETAILED));
        printf(SOS_GRN "         Commit head: " SOS_CLR "%s\n",
                SOS_QUOTE_DEF_STR(GIT_SHA1));
        printf("\n");
        printf(SOS_BOLD_WHT);
        for (col = 0; col < 79; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR SOS_WHT);
        for (col = 0; col < 1; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR "\n" SOS_WHT);
        for (col = 0; col < 1; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR SOS_DIM_WHT);
        for (col = 0; col < 79; col++) { printf(SOS_SYM_GREY_BLOCK); }
        printf(SOS_CLR "\n\n");
    } else {
        // Suppress colors and extended ASCII characters...
        printf("\n");
        printf("             Version: %d.%d\n",
                SOS_VERSION_MAJOR,
                SOS_VERSION_MINOR);
        printf("      Target machine: %s\n",
                SOS_QUOTE_DEF_STR(SOS_HOST_KNOWN_AS));
        printf("\n");
        printf("         Compiled on: "  "%s"  " at "
                "%s" " by " "%s@%s\n",
                __DATE__, __TIME__,
                SOS_QUOTE_DEF_STR(SOS_BUILDER),
                SOS_QUOTE_DEF_STR(SOS_HOST_NODE_NAME));
        printf("   Build environment: "  "%s\n",
                SOS_QUOTE_DEF_STR(SOS_HOST_DETAILED));
        printf("         Commit head: "  "%s\n",
                SOS_QUOTE_DEF_STR(GIT_SHA1));
        printf("\n");
        printf("-----------------------------------------"
               "-----------------------------------------\n");
        printf("========================================="
               "=========================================\n");
        
    } 

    return;
}
