
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
        "                 -r, --role <listener | aggregator>\n" \
        "                 -k, --rank <rank within ALL sosd instances>\n" \
        "\n" \
        "                 NOTE: Aggregator ranks [-k #] need to be contiguous\n"\
        "                       from 0 to n-1 aggregators.\n" \
        "\n" \
        "\n"
#else
    #define OPT_PARAMS "\n"
#endif


#define USAGE          "USAGE:   $ sosd  -l, --listeners <count>\n" \
                       "                 -a, --aggregators <count>\n" \
                       "                [-w, --work_dir <full_path>]\n" \
                       "                [-o, --options_file <full_path]\n"\
                       "                [-c, --options_class <class>]\n"\
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
#include "sos_types.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sos_types.h"
#include "sos_options.h"
#include "sosd.h"
#include "sosd_db_sqlite.h"

#include "sosa.h"

#include "sos_pipe.h"
#include "sos_qhashtbl.h"
#include "sos_buffer.h"
#include "sos_target.h"

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
    SOSD.daemon.log_file    = (char *) calloc(sizeof(char), PATH_MAX);

    // Default to the environment variables.
    // NOTE: This will be overwritten by any commandline arguments.
    char *OPTIONS_file_path  = getenv("SOS_OPTIONS_FILE");
    char *OPTIONS_class_name = getenv("SOS_OPTIONS_CLASS");

    // Default the working directory to the current working directory,
    // can be overridden at the command line with the -w option.
    if (!getcwd(SOSD.daemon.work_dir, PATH_MAX)) {
        fprintf(stderr, "\n\nSTATUS: The getcwd() function did not succeed, make"
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
    char* tmp_port = getenv("SOS_CMD_PORT");
    if ((tmp_port == NULL) || (strlen(tmp_port)) < 2) {
        fprintf(stderr, "\n\nSTATUS: SOS_CMD_PORT evar not set."
                "\nSTATUS: Using default: %s\n",
                SOS_DEFAULT_SERVER_PORT);
        fflush(stderr);
        strncpy(tgt->local_port, SOS_DEFAULT_SERVER_PORT, NI_MAXSERV);
    } else {
      strncpy(tgt->local_port, tmp_port, NI_MAXSERV);
    }
    tgt->port_number = atoi(tgt->local_port);

    tgt->listen_backlog           = 20;
    tgt->buffer_len               = SOS_DEFAULT_BUFFER_MAX;
    tgt->timeout                  = SOS_DEFAULT_MSG_TIMEOUT;
    tgt->local_hint.ai_family     = AF_INET;       // Allow IPv4 or IPv6
    tgt->local_hint.ai_socktype   = SOCK_STREAM;   // _STREAM/_DGRAM/_RAW
    tgt->local_hint.ai_flags      = AI_NUMERICSERV;// Don't invoke namserv.
    tgt->local_hint.ai_protocol   = 0;             // Any protocol
    pthread_mutex_unlock(tgt->send_lock);
    // --- end duplication of SOS_target_init();

    // Process command-line arguments
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
        else if ( (strcmp(argv[elem], "--options_file"    ) == 0)
        ||        (strcmp(argv[elem], "-o"                ) == 0)) {
            OPTIONS_file_path = argv[next_elem];
        }
        else if ( (strcmp(argv[elem], "--options_class"   ) == 0)
        ||        (strcmp(argv[elem], "-c"                ) == 0)) {
            OPTIONS_class_name = argv[next_elem];
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

    SOSD.sos_context->role                 = my_role;
    SOSD.sos_context->config.comm_rank     = my_rank;
    SOSD.sos_context->config.options_file  = OPTIONS_file_path;
    SOSD.sos_context->config.options_class = OPTIONS_class_name;

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

    dlog(1, "Initializing SOSD:\n");
    SOSD.sos_context->config.argc = argc;
    SOSD.sos_context->config.argv = argv;

    dlog(1, "   ... calling SOS_init_existing_runtime(SOSD.sos_context,"
            " %s, SOS_RECEIVES_NO_FEEDBACK, NULL) ...\n",
            SOS_ENUM_STR( SOS->role, SOS_ROLE ));
            //
    SOS_init_existing_runtime(&SOSD.sos_context, my_role,
            SOS_RECEIVES_NO_FEEDBACK, NULL);

    dlog(1, "   ... calling SOSD_init()...\n");
    SOSD_init();
    dlog(1, "   ... done. (SOSD_init + SOS_init are complete)\n");

    if (SOS->config.comm_rank == 0) {
        SOSD_display_logo();
    
        if (SOS_DEBUG >= 0) {
            printf("\nSTATUS: SOSflow compiled with SOS_DEBUG"
                    " enabled!\nSTATUS: This will negatively impact performance.\n\n");
            fflush(stdout);
        }
    }

    dlog(1, "Calling register_signal_handler()...\n");
    if (SOSD_DAEMON_LOG > -1) SOS_register_signal_handler(SOSD.sos_context);

    dlog(1, "Calling daemon_setup_socket()...\n");
    SOSD.net->sos_context = SOSD.sos_context;
    SOSD_setup_socket();

    if (SOS->config.options->db_disabled) {
        dlog(1, "Skipping database initialization..."
                " (SOSD_DB_DISABLED == true)\n");
        // This will ensure val_snaps
        // are free()'ed as they come in,
        // except for cases where
        // the cache is enabled and those
        // snaps are used for the cache.
        SOSD.db.snap_queue = NULL;
    } else {
        dlog(1, "Calling SOSD_db_init_database()...\n");
        SOSD_db_init_database();
    }

    dlog(1, "Initializing the sync framework...\n");

    
    if (SOS->config.options->db_disabled == false) {
        SOSD_sync_context_init(SOS, &SOSD.sync.db,
                sizeof(SOSD_db_task *), SOSD_THREAD_db_sync);
        dlog(1, "   ... SOSD_THREAD_db_sync is ENABLED.\n");
    } else {
        dlog(1, "   ... SOSD_THREAD_db_sync is DISABLED.\n");
    }

    #ifdef SOSD_CLOUD_SYNC
    if (SOS->role == SOS_ROLE_LISTENER) {
        SOSD_sync_context_init(SOS, &SOSD.sync.cloud_send,
                sizeof(SOS_buffer *), SOSD_THREAD_cloud_send);
        SOSD_cloud_start();
        dlog(1, "   ... SOSD_THREAD_cloud_send is ENABLED.\n");
    }
    #else
    #endif
    SOSD_sync_context_init(SOS, &SOSD.sync.local, sizeof(SOS_buffer *),
        SOSD_THREAD_local_sync);

    // Do system monitoring, if requested.
    if (SOS->config.options->system_monitor_enabled) {
        dlog(1, "   ... automatic monitoring of client PIDs is ENABLED.\n");
        SOSD_sync_context_init(SOS, &SOSD.sync.system_monitor, 0,
             SOSD_THREAD_system_monitor);
    } else {
        dlog(1, "   ... automatic monitoring of client PIDs is DISABLED.\n");
    }

    SOSD_sync_context_init(SOS, &SOSD.sync.feedback,
            sizeof(SOSD_feedback_task *), SOSD_THREAD_feedback_sync);

    SOSD.sync.sense_list_lock = calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(SOSD.sync.sense_list_lock, NULL);

    dlog(1, "Entering listening loops...\n");

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
    // Start listening to the socket with the main thread:
    //
    SOSD_listen_loop();
    //
    // Wait for the database to be done flushing...
    //
    if (SOS->config.options->db_disabled == false) {
        pthread_join(*SOSD.sync.db.handler, NULL);
    }
    //
    // Done!  Cleanup and shut down.
    //

    // If requested, export the database to a file:
    if ((SOS->config.options->db_export_at_exit) 
     && (SOS->config.options->db_disabled == false)) 
    {
        char dest_file[PATH_MAX] = {0};
        //Go with the default naming convention:
        snprintf(dest_file, PATH_MAX, "%s/%s.%05d.db",
                SOSD.daemon.work_dir,
                SOSD.daemon.name,
                SOS->config.comm_rank);
        if (SOS_file_exists(dest_file)) {
            //If this file exists already and we're using an in-memory-only dabase
            //then remove the file, otherwise append a qualifier to avoid issues
            //with exporting while an existing file-based DB is in use.
            //(i.e. user config error)
            if (SOS->config.options->db_in_memory_only) {
                remove(dest_file);
            } else {
                snprintf(dest_file, PATH_MAX, "%s/%s.%05d.db.export",
                        SOSD.daemon.work_dir,
                        SOSD.daemon.name,
                        SOS->config.comm_rank);
            }
        }
        dlog(1, "Exporting database to file:  %s\n", dest_file);
        SOSD_db_export_to_file(dest_file);
        dlog(1, "Exporting complete.\n");
    } // end: if DB enabled

    // TODO: Send a trigger to the "SOS" channel to provide notice of shutdown...
    // This is a really nice idea for coordinated shutdown. 


    dlog(1, "Closing the sync queues:\n");

    if (SOSD.sync.local.queue != NULL) {
        dlog(1, "  .. SOSD.sync.local.queue\n");
        pipe_producer_free(SOSD.sync.local.queue->intake);
    }
    if (SOSD.sync.cloud_send.queue != NULL) {
        dlog(1, "  .. SOSD.sync.cloud_send.queue\n");
        pipe_producer_free(SOSD.sync.cloud_send.queue->intake);
    }
    if (SOSD.sync.cloud_recv.queue != NULL) {
        dlog(1, "  .. SOSD.sync.cloud_recv.queue\n");
        pipe_producer_free(SOSD.sync.cloud_recv.queue->intake);
    }
    if ((SOSD.sync.db.queue != NULL)
     && (SOS->config.options->db_disabled == false))
    {
        dlog(1, "  .. SOSD.sync.db.queue\n");
        pipe_producer_free(SOSD.sync.db.queue->intake);
    }
    if (SOSD.sync.feedback.queue != NULL) {
        dlog(1, "  .. SOSD.sync.feedback.queue\n");
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

    dlog(1, "Destroying uid configurations.\n");
    SOS_uid_destroy( SOSD.guid );
    dlog(1, "  ... done.\n");
    if (SOS->config.options->db_disabled == false) {
        dlog(1, "Closing the database.\n");
        SOSD_db_close_database();
    }
    dlog(1, "Closing the socket.\n");
    shutdown(SOSD.net->local_socket_fd, SHUT_RDWR);
    #if (SOSD_CLOUD_SYNC > 0)
    dlog(1, "Detaching from the cloud of sosd daemons.\n");
    SOSD_cloud_finalize();
    #endif

    dlog(1, "Shutting down SOS services.\n");
    SOS_finalize(SOS);

    if (SOSD_DAEMON_LOG >= 0) { fclose(sos_daemon_log_fptr); }
    if (SOSD_DAEMON_LOG >= 0) { free(SOSD.daemon.log_file); }
    if (SOS_DEBUG > 0)   {
        pthread_mutex_lock(SOSD.daemon.countof.lock_stats);
        pthread_mutex_destroy(SOSD.daemon.countof.lock_stats);
        free(SOSD.daemon.countof.lock_stats);
    }

    free(SOSD.daemon.name);

    fprintf(stdout, "** SHUTDOWN **: sosd(%d) is exiting cleanly!\n", my_rank);
    fflush(stdout);

    return(EXIT_SUCCESS);
} //end: main()



// The main loop for listening to the on-node socket.
// Messages received here are placed in the local_sync queue
//     for processing, so we can go back and grab the next
//     socket message ASAP.
void SOSD_listen_loop() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_listen_loop");
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

    dlog(1, "Entering main loop...\n");
    while (SOSD.daemon.running) {
        SOS_buffer_wipe(buffer);

        dlog(5, "Listening for a message...\n");
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

        i = SOS_target_accept_connection(SOSD.net);
        while (i < 0) {
            dlog(0, "WARNING: Unable to accept a connection on port %d!\n",
                    SOSD.net->port_number);
            SOSD.net->port_number += 1;
            snprintf(SOSD.net->local_port, NI_MAXSERV, "%d",
                    SOSD.net->port_number);
            dlog(0, "WARNING: Automatically moving to the next port: %d ...\n",
                    SOSD.net->port_number);
            SOSD_setup_socket();
            i = SOS_target_accept_connection(SOSD.net);
        }


        dlog(5, "Accepted connection.  Attempting to receive message...\n");
        i = SOS_target_recv_msg(SOSD.net, buffer);
        if (i < sizeof(SOS_msg_header)) {
            printf(".");
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

        case SOS_MSG_TYPE_ECHO:         SOSD_handle_echo        (buffer); break;
        case SOS_MSG_TYPE_SHUTDOWN:     SOSD_handle_shutdown    (buffer); break;
        case SOS_MSG_TYPE_CHECK_IN:     SOSD_handle_check_in    (buffer); break;
        case SOS_MSG_TYPE_PROBE:        SOSD_handle_probe       (buffer); break;
        case SOS_MSG_TYPE_QUERY:        SOSD_handle_query       (buffer); break;
        case SOS_MSG_TYPE_CACHE_GRAB:   SOSD_handle_cache_grab  (buffer); break;
        case SOS_MSG_TYPE_CACHE_SIZE:   SOSD_handle_cache_size  (buffer); break;
        case SOS_MSG_TYPE_SENSITIVITY:  SOSD_handle_sensitivity (buffer); break;
        case SOS_MSG_TYPE_DESENSITIZE:  SOSD_handle_desensitize (buffer); break;
        case SOS_MSG_TYPE_TRIGGERPULL:  SOSD_handle_triggerpull (buffer); break;
        default:                        SOSD_handle_unknown     (buffer); break;
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
 
    // NOTE: This calls SOSD_setup_system_monitor_pub() internally:
    //
    SOSD_setup_system_data();
    //

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

    return NULL; //Stops the PGI compiler from complaining.
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

    // For processing cache_grabs...
    SOSD_cache_grab_handle *cache;
    // For processing queries...
    SOSD_query_handle *query;
    // For processing payloads...
    SOSD_feedback_payload *payload = NULL;
    SOS_buffer *delivery = NULL;
    SOS_buffer_init_sized_locking(SOS, &delivery, 1024, false);

    SOS_socket *target = NULL;

    SOS_buffer *results_reply_msg = NULL;
    SOS_buffer_init(SOS, &results_reply_msg);

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

        case SOS_FEEDBACK_TYPE_CACHE:
            //>>>>>
            dlog(6, "Processing feedback message for cache_grab!\n");
            cache = (SOSD_cache_grab_handle *) task->ref;
            
            rc = SOS_target_init(SOS, &target,
                    cache->reply_host, cache->reply_port);
            if (rc != 0) {
                dlog(0, "ERROR: Unable to initialize link to %s:%d"
                        " ... destroying results and returning.\n",
                        cache->reply_host,
                        cache->reply_port);
                if (cache->reply_host != NULL) {
                    free(cache->reply_host);
                    cache->reply_host = NULL;
                }
                SOSA_results_destroy(cache->results);
                free(cache);
                free(task);
                continue;
            }

            dlog(5, "SOSD: Sending cache results to %s:%d ...\n",
                   cache->reply_host,
                   cache->reply_port);

            rc = SOS_target_connect(target);
            if (rc != 0) {
                dlog(0, "Unable to connect to"
                        " client at %s:%d\n",
                        cache->reply_host,
                        cache->reply_port);
            }

            SOS_buffer_wipe(results_reply_msg);
            SOSA_results_to_buffer(results_reply_msg, cache->results);

            rc = SOS_target_send_msg(target, results_reply_msg);
            if (rc < 0) {
                dlog(0, "SOSD: Unable to send message to client.\n");
            }

            rc = SOS_target_disconnect(target);
            rc = SOS_target_destroy(target);
            if (cache->reply_host != NULL) {
                free(cache->reply_host);
                cache->reply_host = NULL;
            }

            //Done delivering cache results.
            SOSA_results_destroy(cache->results);
            free(cache);

            dlog(6, "Done processing feedback for cache_grab.\n");
            //<<<<<
            break;


        case SOS_FEEDBACK_TYPE_QUERY:
            query = (SOSD_query_handle *) task->ref;
            
            rc = SOS_target_init(SOS, &target,
                    query->reply_host, query->reply_port);
            if (rc != 0) {
                dlog(0, "ERROR: Unable to initialize link to %s:%d"
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

            dlog(5, "SOSD: Sending query results to %s:%d ...\n",
                   query->reply_host,
                   query->reply_port);

            rc = SOS_target_connect(target);
            if (rc != 0) {
                dlog(0, "Unable to connect to"
                        " client at %s:%d\n",
                        query->reply_host,
                        query->reply_port);
            }

            SOS_buffer_wipe(results_reply_msg);
            SOSA_results_to_buffer(results_reply_msg, query->results);

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
            SOSA_results_destroy(query->results);
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
                        //fprintf(stderr, "Removing missing target --> %s:%d\n",
                        //        sense->remote_host,
                        //        sense->remote_port);
                        //fflush(stderr);
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

    return NULL; //Stops the PGI compiler from complaining.
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
    
    return NULL; //Stops the PGI compiler from complaining.
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

    if (SOS->config.options->db_disabled == true) {
        pthread_exit(NULL);
    }

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

    return NULL; //Stops the PGI compiler from complaining.
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
            dlog(1, "Nothing in the queue and the intake is closed."
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
                dlog(4, "(header.msg_size == %d   +   offset == %d)"
                        "  >  buffer->max == %d    (growing...)\n",
                     header.msg_size, offset, buffer->max);
                SOS_buffer_grow(buffer, buffer->max, SOS_WHOAMI);
            }

            dlog(4, "(%d of %d) --> packing in "
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
        dlog(4, "Sending %d messages in %d bytes"
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

    return NULL; //Stops the PGI compiler from complaining.
}


/* -------------------------------------------------- */



void
SOSD_send_to_self(SOS_buffer *send_buffer, SOS_buffer *reply_buffer) {
    SOS_SET_CONTEXT(send_buffer->sos_context, "SOSD_send_to_self");

    dlog(1, "Preparing to send a message to our own listening socket...\n");
    dlog(1, "  ... Initializing...\n");
    SOS_socket *my_own_listen_port = NULL;
    const char * portStr = getenv("SOS_CMD_PORT");
    if (portStr == NULL) { portStr = SOS_DEFAULT_SERVER_PORT; }
    SOS_target_init(SOS, &my_own_listen_port, "localhost", atoi(portStr));
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
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_desensitize");

    SOS_msg_header header;
    int rc;

    // TODO: Remove the specific sensitivity GUID+handle combo.
    //       This is currently handled

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
SOSD_handle_cache_grab(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_cache_grab");

    SOS_msg_header header;

    int offset = 0;
    SOS_msg_unzip(msg, &header, 0, &offset);

    SOSD_cache_grab_handle *cache_grab = calloc(1, sizeof(SOSD_cache_grab_handle));
    cache_grab->results = NULL;
    SOSA_results_init(SOSD.sos_context, &cache_grab->results);

    SOS_buffer_unpack_safestr(msg, &offset, &cache_grab->reply_host);
    SOS_buffer_unpack(msg, &offset, "i", &cache_grab->reply_port);
    SOS_buffer_unpack_safestr(msg, &offset, &cache_grab->pub_filter_regex);
    SOS_buffer_unpack_safestr(msg, &offset, &cache_grab->val_filter_regex);
    SOS_buffer_unpack(msg, &offset, "i", &cache_grab->frame_head);
    SOS_buffer_unpack(msg, &offset, "i", &cache_grab->frame_depth_limit);
    SOS_buffer_unpack(msg, &offset, "g", &cache_grab->req_guid);
   
    char filters[2048];
    snprintf(filters, 2048, "pub:\"%s\" val:\"%s\"",
            cache_grab->pub_filter_regex,
            cache_grab->val_filter_regex);

    SOSA_results_label(cache_grab->results, cache_grab->req_guid, filters);

    // NOTE: Immediately service this cache grab operation, keep an eye on this.
    SOSA_cache_to_results(SOSD.sos_context, cache_grab->results,
        cache_grab->pub_filter_regex, cache_grab->val_filter_regex,
        cache_grab->frame_head, cache_grab->frame_depth_limit);
    
    SOSD_feedback_task *new_task =
        (SOSD_feedback_task *) calloc(1, sizeof(SOSD_feedback_task));

    new_task->type = SOS_FEEDBACK_TYPE_CACHE;
    new_task->ref = cache_grab;
   
    // Enqueue this in the feedback pipeline:
    dlog(6, "   ...enqueing results into feedback pipeline.  (%d rows)\n",
            cache_grab->results->row_count);
    pthread_mutex_lock(SOSD.sync.feedback.queue->sync_lock);
    pipe_push(SOSD.sync.feedback.queue->intake, (void *) &new_task, 1);
    SOSD.sync.feedback.queue->elem_count++;
    pthread_mutex_unlock(SOSD.sync.feedback.queue->sync_lock);

    dlog(6, "   ...send ACK to client.\n");
    SOS_buffer *reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);
    SOSD_PACK_ACK(reply);
    int rc = SOS_target_send_msg(SOSD.net, reply);
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


void
SOSD_handle_cache_size(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_cache_size");

    SOS_msg_header header;
    int offset = 0;

    bool cache_resized = false;

    // Start off by releasing the client w/an ACK message...
    dlog(6, "   ...send ACK to client.\n");
    SOS_buffer *reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);
    SOSD_PACK_ACK(reply);
    int rc = SOS_target_send_msg(SOSD.net, reply);
    dlog(5, "replying with reply->len == %d bytes, rc == %d\n",
            reply->len, rc);
    if (rc == -1) {
        dlog(0, "Error sending a response.  (%s)\n", strerror(errno));
    } else {
        SOSD_countof(socket_bytes_sent += rc);
    }
    SOS_buffer_destroy(reply);


    // NOTE: For now, we handle this immediately.
    //       Resizing a pub's cache is a rare event and
    //       usually at init or outside of application loops.
    //
    //       Keep an eye here if the cache concept in SOS
    //       develops in complexity, the guts of this function
    //       could get moved into a work ticket handled
    //       by the feedback thread, and all we do here
    //       is generate the ticket and move on.

    SOS_msg_unzip(msg, &header, 0, &offset);
  
    SOS_pub   *pub           = NULL;
    SOS_guid   guid          = 0;
    char       guid_str[124] = {0};
    int        cache_to_size = 0;

    SOS_buffer_unpack(msg, &offset, "g", &guid);
    SOS_buffer_unpack(msg, &offset, "i", &cache_to_size);

    // IDEA: Allow for a guid of -1 to mean "resize cache for ALL pubs."
   
    if (cache_to_size < 0) cache_to_size = 0;

    snprintf(guid_str, 124, "%" SOS_GUID_FMT, guid);

    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, guid_str);

    if (pub == NULL) {
        dlog(1, "Cache size msg received for unknown pub!  Doing nothing.\n");
        return;
    }

    pthread_mutex_lock(pub->lock);

    if (pub->cache_depth < cache_to_size) {
        // Allow our cache depth to grow automatically as new
        // values arrive.  (requires no immediate effort)
        dlog(4, "Growing pub->cache_depth from %d to %d.\n",
                pub->cache_depth, cache_to_size);
        pub->cache_depth = cache_to_size;
        cache_resized = true;
    } else {
        // Cache is being requested to shrink. Set the new size
        // and then go inside and prune off any data beyond the new
        // depth.
        dlog(4, "Shrinking pub->cache_depth from %d to %d.\n",
                pub->cache_depth, cache_to_size);
        pub->cache_depth = cache_to_size;
        cache_resized = true;

        int           elem = 0;
        int           deep = 0;
        SOS_val_snap *snap = NULL;
        SOS_val_snap *next = NULL;
        SOS_val_snap *last = NULL;

        for (elem = 0; elem < pub->elem_count; elem++) {
            snap = pub->data[elem]->cached_latest;
            deep = -1;
            while ((deep < pub->cache_depth)
                && (snap != NULL)) {
                // Roll through snaps until we find the end of
                // data or go beyond the new limit.
                last = snap;
                snap = snap->next_snap;
                deep++;
            }
            if ((deep >= pub->cache_depth)
                && (snap != NULL)) {
                dlog(4, "Pruning...\n");
                // We have a pointer to the beginning of the
                // cache where we want to start removing values,
                // AND there are values here to remove.
                //
                // Set the previous pointer's next_snap to NULL
                // since it's the new end of the list
                last->next_snap = NULL;
                // Now proceed to wipe out the remainder of the snaps:
                while (snap != NULL) {
                    switch(snap->type) {
                    case SOS_VAL_TYPE_INT:    snap->val.i_val = 0;   break;
                    case SOS_VAL_TYPE_LONG:   snap->val.l_val = 0;   break;
                    case SOS_VAL_TYPE_DOUBLE: snap->val.d_val = 0.0; break;
                    case SOS_VAL_TYPE_STRING: free(snap->val.c_val); break;
                    case SOS_VAL_TYPE_BYTES:  free(snap->val.bytes); break;
                    }
                    dlog(4, "    pub->data[%d]...snap->frame == %d\n",
                            elem, snap->frame);
                    next = snap->next_snap;
                    snap->next_snap = NULL;
                    free(snap);     
                    snap = next;
                } //end: while (snap!=NULL)
            } //end: if (snaps to free)
        } //end: for (all elems in pub)
        //
        // Done shrinking cache.

    } //end: if (pub cache being changed)

    pthread_mutex_unlock(pub->lock);

    if ((cache_resized)
        && (SOS->role != SOS_ROLE_AGGREGATOR)) {
        //Enqueue this message to send to our aggregator
        SOSD_cloud_send(msg, (SOS_buffer *) NULL);
    }


    dlog(6, "Done.\n");

    return;
}

void
SOSD_handle_unregister(SOS_buffer *msg) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSD_handle_unregister");

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
    SOSD_cloud_handle_triggerpull(msg);
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

    if (SOS->config.options->db_disabled) {
        // DB is DISABLED
        //
        // Since the database is disabled (and clients might not know
        // this) we need to take a different response here than to
        // queue it up into the database work order system, as none of
        // that has been initialized.
        //
        // Construct an empty response, drop it in the feedback queue,
        // and return.  Unblock clients who are waiting for results.
      
        dlog(6, "   ...DB is disabled, assembling empty set to reply.\n");

        SOSA_results *results = NULL;
        SOSA_results_init(SOS, &results); 
        SOSA_results_label(results,
                query_handle->query_guid, query_handle->query_sql);
        results->col_count     = 0;
        results->row_count     = 0;
        results->exec_duration = 0.0;

        query_handle->results = results;

        // NOTE: For debugging.  These should not be needed.
        //SOSA_results_put_name(results, 0, "<error>");
        //SOSA_results_put(results, 0, 0, "<db disabled>");

        SOSD_feedback_task *feedback = calloc(1, sizeof(SOSD_feedback_task));
        feedback->type = SOS_FEEDBACK_TYPE_QUERY;
        feedback->ref = query_handle;
        pthread_mutex_lock(SOSD.sync.feedback.queue->sync_lock);
        pipe_push(SOSD.sync.feedback.queue->intake, (void *) &feedback, 1);
        SOSD.sync.feedback.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.feedback.queue->sync_lock);
        
    } else {
        // DB is ENABLED
        dlog(6, "   ...placing query in DB queue.\n");
        SOSD_db_task *task = NULL;
        task = (SOSD_db_task *) calloc(1, sizeof(SOSD_db_task));
        task->ref = (void *) query_handle;
        task->type = SOS_MSG_TYPE_QUERY;
        pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
        pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
        SOSD.sync.db.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);

    }

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
    dlog(5, "header.msg_type = SOS_MSG_TYPE_ECHO\n");

    SOS_msg_header header;
    int            rc;

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

    if (SOS->config.options->db_disabled == false) {
        dlog(5, "Injecting snaps into SOSD.db.snap_queue...\n");
        SOS_val_snap_queue_from_buffer(buffer, SOSD.db.snap_queue, pub);
    } else {
        dlog(5, "Processing snaps into pub/cache... (DB is disabled)\n");
        SOS_val_snap_queue_from_buffer(buffer, NULL, pub);
    }

    if (SOS->config.options->db_disabled == false) {
        dlog(5, "Queue these val snaps up for the database...\n");
        task = (SOSD_db_task *) malloc(sizeof(SOSD_db_task));
        task->ref = (void *) pub;
        task->type = SOS_MSG_TYPE_VAL_SNAPS;
        pthread_mutex_lock(SOSD.sync.db.queue->sync_lock);
        pipe_push(SOSD.sync.db.queue->intake, (void *) &task, 1);
        SOSD.sync.db.queue->elem_count++;
        pthread_mutex_unlock(SOSD.sync.db.queue->sync_lock);
        dlog(5, "  ... done.\n");
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

    int client_uid = -1;
    SOS_buffer_unpack(buffer, &offset, "i",
            &client_uid);

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

    int server_uid = getuid();
    if (client_uid != server_uid) {
        fprintf(stderr, "CRITICAL WARNING: Client UID (%d) does not match daemon's"
                " (%d).  Refusing connection.\n",
                client_uid, server_uid);
        // We're refusing this connection, don't give them GUIDs.
        guid_block_from = 0;
        guid_block_to   = 0;
        // Assemble message so client knows to error-out.
        offset = 0;
        SOS_msg_zip(reply, header, 0, &offset);
        SOS_buffer_pack(reply, &offset, "ggiii",
            guid_block_from,
            guid_block_to,
            SOS_VERSION_MAJOR,
            SOS_VERSION_MINOR,
            server_uid);
        // Reapply the header now that we know the message size.
        header.msg_size = offset;
        offset = 0;
        SOS_msg_zip(reply, header, 0, &offset);
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
        dlog(3, "REGISTER: Providing client with GUIDs _from=%" SOS_GUID_FMT
                ", _to=%" SOS_GUID_FMT "\n", guid_block_from, guid_block_to);
        //Pack in the GUID's
        SOS_buffer_pack(reply, &offset, "gg",
                guid_block_from,
                guid_block_to);

        //Pack in the server's version # (just in case)
        SOS_buffer_pack(reply, &offset, "ii",
                SOS_VERSION_MAJOR,
                SOS_VERSION_MINOR);

        SOS_buffer_pack(reply, &offset, "i",
                server_uid);

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
        // If it's not in the table, add it. 
        SOS_pub_init(SOS, &pub,pub_guid_str, SOS_NATURE_DEFAULT);
        SOSD_countof(pub_handles++);
        strncpy(pub->guid_str,pub_guid_str, SOS_DEFAULT_STRING_LEN);
        pub->guid = header.ref_guid;
        SOSD.pub_table->put(SOSD.pub_table,pub_guid_str, pub);
        // Add a pointer to the pub_list:
        SOS_list_entry *new_entry = calloc(1, sizeof(SOS_list_entry));
        new_entry->ref = (void *)pub;
        new_entry->next_entry = SOSD.pub_list_head;
        SOSD.pub_list_head = new_entry;
        // Make sure the pub only goes in once...
        firstAnnouncement = true;
    } else {
        dlog(5, "     ... FOUND IT!\n");
        firstAnnouncement = false;
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n",
            SOSD.pub_table->size(SOSD.pub_table));
    dlog(5, "Calling SOSD_apply_announce() ...\n");

    //
    SOSD_apply_announce(pub, buffer);
    //
    pub->announced = SOSD_PUB_ANN_DIRTY;
    //

    if ((firstAnnouncement == true) 
     && (SOS->config.options->db_disabled == false)) 
    {
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
    // Check the table for this pub ...
    dlog(5, "  ... checking SOS->pub_table for GUID(%s):\n",pub_guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table,pub_guid_str);

    if (pub == NULL) {
        // If it's not in the table, add it.
        dlog(0, "ERROR: PUBLISHING INTO A PUB (guid:%" SOS_GUID_FMT
            ") NOT FOUND! (WEIRD!)\n", header.ref_guid);
        dlog(0, "ERROR: .... ADDING previously unknown pub to the table..."
            " (this is bogus, man)\n");
        SOS_pub_init(SOS, &pub,pub_guid_str, SOS_NATURE_DEFAULT);
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

    if (SOS->config.options->db_disabled == false) {
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
        } //end: if sync_pending
    } //end: if db enabled

    return;
}



void SOSD_handle_shutdown(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSD_handle_shutdown");
    SOS_msg_header header;
    SOS_buffer    *reply;
    int            offset;
    int            i;

    dlog(1, "header.msg_type = SOS_MSG_TYPE_SHUTDOWN\n");

    //if (SOS->role == SOS_ROLE_AGGREGATOR) {
    //    fprintf(stderr, "WARNING: Attempt to trigger shutdown"
    //            " at AGGREGATOR.\n");
    //    dlog(0, "CRITICAL WARNING: Shutdown was triggered on an AGGREGATION\n"
    //            "    daemon. Shutdown should be sent to each in situ LISTENER,\n"
    //            "    and they will automatically propagate the notification via\n"
    //            "    inter-daemon communication channels to the appropriate\n"
    //            "    aggregation targets, if any exist.\n"
    //            "    ... attempting to proceed anyway.\n");
    //}

    reply = NULL;
    SOS_buffer_init_sized(SOS, &reply, SOS_DEFAULT_REPLY_LEN);

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

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
#if (SOSD_CLOUD_SYNC > 0)
    if (SOS->role == SOS_ROLE_LISTENER) {
        // Listeners determine if they need to relay the
        // shutdown notice to Aggregators...
        SOSD_cloud_shutdown_notice();
    }
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

    uint64_t queue_depth_db_tasks  = 0;
    uint64_t queue_depth_db_snaps  = 0;
    if (SOS->config.options->db_disabled == false) {
        queue_depth_db_tasks  = SOSD.sync.db.queue->elem_count;
        queue_depth_db_snaps  = SOSD.db.snap_queue->elem_count;
    }

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
    SOS_target_setup_for_accept(SOSD.net);
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
    SOSD.pub_table      = qhashtbl(SOS_DEFAULT_TABLE_SIZE);
    SOSD.pub_list_head  = NULL;

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
        dlog(6, "served GUID block: %" SOS_GUID_FMT " ----> %"
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
    if (SOS->config.options->system_monitor_enabled) {
        SOSD_add_pid_to_track(pub);
    }

    return;
}


void SOSD_apply_publish( SOS_pub *pub, SOS_buffer *buffer ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_publish");

    dlog(6, "Calling SOS_publish_from_buffer()...\n");
    if (SOS->config.options->db_disabled) {
        SOS_publish_from_buffer(buffer, pub, SOSD.db.snap_queue);
    } else {
        SOS_publish_from_buffer(buffer, pub, NULL);
    }

    return;
}





void SOSD_display_logo(void) {

    int col = 0;
    int choice = 0;
    srand(getpid());
    choice = rand() % 3;

    if (SOS_str_opt_is_enabled(getenv("SOS_BATCH_ENVIRONMENT"))) {
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


    if (!SOS_str_opt_is_enabled(getenv("SOS_BATCH_ENVIRONMENT"))) {
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
    fflush(stdout);
    return;
}
