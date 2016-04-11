/*
 *  sosd.c (daemon)
 *
 *
 *
 */


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
#include "qhashtbl.h"
#include "pack_buffer.h"

#define USAGE          "usage:   $ sosd  --port <number>  --buffer_len <bytes>  --listen_backlog <len>  --role <role>  --work_dir <path>"

void SOSD_display_logo(void);

int main(int argc, char *argv[])  {
    int elem, next_elem;
    int retval;
    SOS_role my_role;

    SOSD.daemon.work_dir    = (char *) &SOSD_DEFAULT_DIR;
    SOSD.daemon.name        = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
    SOSD.daemon.lock_file   = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);
    SOSD.daemon.log_file    = (char *) calloc(sizeof(char), SOS_DEFAULT_STRING_LEN);

    my_role = SOS_ROLE_DAEMON; /* This can be overridden by command line argument. */

    /* Process command-line arguments */
    if ( argc < 7 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }
    SOSD.net.port_number    = -1;
    SOSD.net.buffer_len     = -1;
    SOSD.net.listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) { fprintf(stderr, "%s\n", USAGE); exit(1); }
        if (      strcmp(argv[elem], "--port"            ) == 0) { SOSD.net.server_port    = argv[next_elem];       }
        else if ( strcmp(argv[elem], "--buffer_len"      ) == 0) { SOSD.net.buffer_len     = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--listen_backlog"  ) == 0) { SOSD.net.listen_backlog = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--work_dir"        ) == 0) { SOSD.daemon.work_dir    = argv[next_elem];       }
        else if ( strcmp(argv[elem], "--role"            ) == 0) {
            if (      strcmp(argv[next_elem], "SOS_ROLE_DAEMON" ) == 0)  { my_role = SOS_ROLE_DAEMON; }
            else if ( strcmp(argv[next_elem], "SOS_ROLE_DB" ) == 0)      { my_role = SOS_ROLE_DB; }
            else if ( strcmp(argv[next_elem], "SOS_ROLE_CONTROL" ) == 0) { my_role = SOS_ROLE_CONTROL; }
            else {  fprintf(stderr, "Unknown role: %s %s\n", argv[elem], argv[next_elem]); }
        } else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    SOSD.net.port_number = atoi(SOSD.net.server_port);
    if ( (SOSD.net.port_number < 1)
         || (SOSD.net.buffer_len < 1)
         || (SOSD.net.listen_backlog < 1) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }

    #ifndef SOSD_CLOUD_SYNC
    if (my_role != SOS_ROLE_DAEMON) {
        printf("NOTE: Terminating an instance of sosd with pid: %d\n", getpid());
        printf("NOTE: SOSD_CLOUD_SYNC is disabled but this instance is not a SOS_ROLE_DAEMON!\n");
        fflush(stdout);
        exit(EXIT_FAILURE);
    }
    #endif

    memset(&SOSD.daemon.pid_str, '\0', 256);

    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("Preparing to initialize:\n"); fflush(stdout); }
    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("   ... creating SOS_runtime object for daemon use.\n"); fflush(stdout); }
    SOSD.sos_context = (SOS_runtime *) malloc(sizeof(SOS_runtime));
    memset(SOSD.sos_context, '\0', sizeof(SOS_runtime));
    SOSD.sos_context->role = my_role;

    #ifdef SOSD_CLOUD_SYNC
    if (SOSD_DAEMON_LOG && SOSD_ECHO_TO_STDOUT) { printf("   ... calling SOSD_cloud_init()...\n"); fflush(stdout); }
    SOSD_cloud_init( &argc, &argv);
    #else
    dlog(0, "   ... WARNING: There is no CLOUD_SYNC configured for this SOSD.\n");
    #endif

    SOS_SET_CONTEXT(SOSD.sos_context, "main");
    dlog(0, "Initializing SOSD:\n");

    dlog(0, "   ... calling SOS_init(argc, argv, %s, SOSD.sos_context) ...\n", SOS_ENUM_STR( my_role, SOS_ROLE ));
    SOSD.sos_context = SOS_init_runtime( &argc, &argv, my_role, SOS_LAYER_SOS_RUNTIME, SOSD.sos_context );

    dlog(0, "   ... calling SOSD_init()...\n");
    SOSD_init();
    if (SOS->config.comm_rank == 0) {
        SOSD_display_logo();
    }

    dlog(0, "   ... done. (SOSD_init + SOS_init are complete)\n");
    dlog(0, "Calling register_signal_handler()...\n");
    if (SOS_DEBUG) SOS_register_signal_handler(SOSD.sos_context);
    if (SOS->role == SOS_ROLE_DAEMON) {
        dlog(0, "Calling daemon_setup_socket()...\n");
        SOSD_setup_socket();
    }

    dlog(0, "Calling daemon_init_database()...\n");
    SOSD_db_init_database();
    dlog(0, "Creating the val_snap queues.\n");
    SOS_val_snap_queue_init(SOS, &SOS->task.val_intake);
    SOS_val_snap_queue_init(SOS, &SOS->task.val_outlet);
    dlog(0, "Creating ring queue monitors to track 'to-do' list for pubs...\n");
    #ifdef SOSD_CLOUD_SYNC
    /* Start LOCAL and CLOUD sync threads... */
    SOSD_pub_ring_monitor_init(&SOSD.cloud_sync, "cloud_sync", NULL, SOS->task.val_outlet, NULL, SOS_TARGET_CLOUD_SYNC);
    SOSD_pub_ring_monitor_init(&SOSD.local_sync, "local_sync", NULL, SOS->task.val_intake, SOS->task.val_outlet, SOS_TARGET_LOCAL_SYNC);
    dlog(0, "Releasing the cloud_sync (flush) thread to begin operation...\n");
    pthread_cond_signal(SOSD.cloud_bp->flush_cond);
    #else
    /*   ...or, start only a LOCAL sync thread. */
    SOSD_pub_ring_monitor_init(&SOSD.local_sync, "local_sync", NULL, SOS->task.val_intake, NULL, SOS_TARGET_LOCAL_SYNC);
    #endif

    dlog(0, "Entering listening loop...\n");

    /* Go! */
    switch (SOS->role) {
    case SOS_ROLE_DAEMON:   SOSD_listen_loop(); break;
    case SOS_ROLE_DB:       
        #ifdef SOSD_CLOUD_SYNC
        SOSD_cloud_listen_loop();
        #endif
        break;
    case SOS_ROLE_CONTROL:  break;
    default: break;
    }

    /* Done!  Cleanup and shut down. */
    dlog(0, "Ending the pub_ring monitors:\n");
    dlog(0, "  ... waiting for the pub_ring monitor to iterate and exit.\n");
    pthread_join( (SOSD.local_sync->extract_t), NULL);
    pthread_join( (SOSD.local_sync->commit_t), NULL);
    #if (SOSD_CLOUD_SYNC > 0)
    pthread_join( (SOSD.cloud_sync->extract_t), NULL);
    pthread_join( (SOSD.cloud_sync->commit_t), NULL);
    #endif

    dlog(0, "  ... destroying the ring monitors...\n");
    SOSD_pub_ring_monitor_destroy(SOSD.local_sync);
    #if (SOSD_CLOUD_SYNC > 0)
    SOSD_pub_ring_monitor_destroy(SOSD.cloud_sync);
    #endif

    dlog(0, "  ... destroying the val_snap queues...\n");
    SOS_val_snap_queue_destroy(SOS->task.val_intake);
    SOS_val_snap_queue_destroy(SOS->task.val_outlet);
    dlog(0, "  ... destroying uid configurations.\n");
    SOS_uid_destroy( SOSD.guid );
    dlog(0, "  ... done.\n");
    dlog(0, "Closing the database.\n");
    SOSD_db_close_database();
    if (SOS->role == SOS_ROLE_DAEMON) {
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

    close(sos_daemon_lock_fptr);
    remove(SOSD.daemon.lock_file);

    free(SOSD.daemon.name);
    free(SOSD.daemon.lock_file);

    return(EXIT_SUCCESS);
} //end: main()



void SOSD_pub_ring_monitor_init(SOSD_pub_ring_mon **mon_var,
                                char *name_var,
                                SOS_ring_queue *ring_var,
                                SOS_val_snap_queue *val_source,
                                SOS_val_snap_queue *val_target,
                                SOS_target target )
{
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_pub_ring_monitor_init");
    SOSD_pub_ring_mon *mon;
    int retval;

    mon = *mon_var = malloc(sizeof(SOSD_pub_ring_mon));
    memset(mon, '\0', sizeof(SOSD_pub_ring_mon));

    mon->name = name_var;
    if (ring_var == NULL) {
        SOS_ring_init(SOS, &mon->ring);
    } else {
        mon->ring = ring_var;
    }

    mon->commit_list   = NULL;
    mon->commit_count  = 0;
    mon->commit_target = target;
    mon->val_intake    = val_source;
    mon->val_outlet    = val_target;

    retval = pthread_cond_init(&(mon->extract_cond), NULL); 
    if (retval != 0) { dlog(0, "ERROR!  Could not initialize the mon(%s)->extract_cond pthread_cond_t variable.  (%s)\n", mon->name, strerror(errno)); exit(EXIT_FAILURE); } 
    retval = pthread_cond_init(&(mon->commit_cond), NULL);
    if (retval != 0) { dlog(0, "ERROR!  Could not initialize the mon(%s)->commit_cond pthread_cond_t variable.  (%s)\n", mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_mutex_init(&(mon->extract_lock), NULL);
    if (retval != 0) { dlog(0, "ERROR!  Could not initialize the mon(%s)->extract_lock mutex.  (%s)\n", mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_mutex_init(&(mon->commit_lock), NULL);
    if (retval != 0) { dlog(0, "ERROR!  Could not initialize the mon(%s)->commit_lock mutex.  (%s)\n", mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_create( &(mon->extract_t), NULL, (void *) SOSD_THREAD_pub_ring_list_extractor, mon );
    if (retval != 0) { dlog(0, "ERROR!  Could not initialize the mon(%s)->extract_t thread.  (%s)\n", mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_create( &(mon->commit_t), NULL, (void *) SOSD_THREAD_pub_ring_storage_injector, mon );
    if (retval != 0) { dlog(0, "ERROR!  Could not initialize the mon(%s)->commit_t thread.  (%s)\n", mon->name, strerror(errno)); exit(EXIT_FAILURE); }

    return;
}

void SOSD_pub_ring_monitor_destroy(SOSD_pub_ring_mon *mon) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_pub_ring_monitor_destroy");

    pthread_mutex_destroy( &(mon->extract_lock) );
    pthread_mutex_destroy( &(mon->commit_lock) );
    pthread_cond_destroy( &(mon->extract_cond) );
    pthread_cond_destroy( &(mon->commit_cond) );

    SOS_ring_destroy(mon->ring);
    free(mon);

    return;
}



/* -------------------------------------------------- */
void SOSD_listen_loop() {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_listen_loop");
    SOS_msg_header header;
    int      i, byte_count, recv_len;

    unsigned char    *buffer;
    buffer = (unsigned char *) malloc(sizeof(unsigned char) * SOSD.net.buffer_len);

    dlog(0, "Entering main loop...\n");
    while (SOSD.daemon.running) {
        memset(buffer, '\0', SOSD.net.buffer_len);
        memset(&header, '\0', sizeof(SOS_msg_header));
        byte_count = 0;

        dlog(5, "Listening for a message...\n");
        SOSD.net.peer_addr_len = sizeof(SOSD.net.peer_addr);
        SOSD.net.client_socket_fd = accept(SOSD.net.server_socket_fd, (struct sockaddr *) &SOSD.net.peer_addr, &SOSD.net.peer_addr_len);
        i = getnameinfo((struct sockaddr *) &SOSD.net.peer_addr, SOSD.net.peer_addr_len, SOSD.net.client_host, NI_MAXHOST, SOSD.net.client_port, NI_MAXSERV, NI_NUMERICSERV);
        if (i != 0) { dlog(0, "Error calling getnameinfo() on client connection.  (%s)\n", strerror(errno)); break; }

        recv_len = recv(SOSD.net.client_socket_fd, (void *) buffer, SOSD.net.buffer_len, 0);
        if (recv_len < 1) {
            dlog(1, "  ... recv() call returned an errror.  (%s)\n", strerror(errno));
            continue;
        }

        byte_count += recv_len;

        if (byte_count >= sizeof(SOS_msg_header)) {

            SOS_buffer_unpack(SOS, buffer, "iill",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.pub_guid);

            dlog(6, "  ... Received %d of %d bytes in this message.\n", byte_count, header.msg_size);
        } else {
            dlog(0, "  ... Received short (useless) message.\n");  continue;
        }

        while (byte_count < header.msg_size) {
            recv_len += recv(SOSD.net.client_socket_fd, (void *) (buffer + byte_count), SOSD.net.buffer_len, 0);
            if (recv_len < 1) {
                dlog(6, "     ... ERROR!  Remote side closed their connection.\n");
                continue;
            } else {
                dlog(6, "     ... %d of %d ...\n", byte_count, header.msg_size);
                byte_count += recv_len;
            }
        }

        dlog(5, "Received connection.\n");
        dlog(5, "  ... byte_count = %d\n", byte_count);
        dlog(5, "  ... msg_from = %ld\n", header.msg_from);

        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER:   dlog(5, "  ... msg_type = REGISTER (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_GUID_BLOCK: dlog(5, "  ... msg_type = GUID_BLOCK (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_ANNOUNCE:   dlog(5, "  ... msg_type = ANNOUNCE (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_PUBLISH:    dlog(5, "  ... msg_type = PUBLISH (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_VAL_SNAPS:  dlog(5, "  ... msg_type = VAL_SNAPS (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_ECHO:       dlog(5, "  ... msg_type = ECHO (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_SHUTDOWN:   dlog(5, "  ... msg_type = SHUTDOWN (%d)\n", header.msg_type); break;
        case SOS_MSG_TYPE_CHECK_IN:   dlog(5, "  ... msg_type = CHECK_IN (%d)\n", header.msg_type); break;
        default:                      dlog(1, "  ... msg_type = UNKNOWN (%d)\n", header.msg_type); break;
        }

        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER:   SOSD_handle_register   (buffer, byte_count); break; 
        case SOS_MSG_TYPE_GUID_BLOCK: SOSD_handle_guid_block (buffer, byte_count); break;
        case SOS_MSG_TYPE_ANNOUNCE:   SOSD_handle_announce   (buffer, byte_count); break;
        case SOS_MSG_TYPE_PUBLISH:    SOSD_handle_publish    (buffer, byte_count); break;
        case SOS_MSG_TYPE_VAL_SNAPS:  SOSD_handle_val_snaps  (buffer, byte_count); break;
        case SOS_MSG_TYPE_ECHO:       SOSD_handle_echo       (buffer, byte_count); break;
        case SOS_MSG_TYPE_SHUTDOWN:   SOSD_handle_shutdown   (buffer, byte_count); break;
        case SOS_MSG_TYPE_CHECK_IN:   SOSD_handle_check_in   (buffer, byte_count); break;
        default:                      SOSD_handle_unknown    (buffer, byte_count); break;
        }

        close( SOSD.net.client_socket_fd );
    }
    free(buffer);
    dlog(1, "Leaving the socket listening loop.\n");

    return;
}

/* -------------------------------------------------- */


void* SOSD_THREAD_pub_ring_list_extractor(void *args) {
    SOSD_pub_ring_mon *my = (SOSD_pub_ring_mon *) args;
    char func_name[SOS_DEFAULT_STRING_LEN] = {0};

    sprintf(func_name, "SOSD_THREAD_pub_ring_extractor(%s)", my->name);
    SOS_SET_CONTEXT(SOSD.sos_context, func_name);

    struct timespec ts;
    struct timeval  tp;
    int wake_type;
    gettimeofday(&tp, NULL); ts.tv_sec  = tp.tv_sec; ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;   /* ~ 0.06 seconds. */
    pthread_mutex_lock(&my->extract_lock);
    while (SOSD.daemon.running) {
        wake_type = pthread_cond_timedwait(&my->extract_cond, &my->extract_lock, &ts);
        if (wake_type == ETIMEDOUT) {
            /* ...any special actions that need to happen if timed-out vs. called-explicitly */
            if (my->ring->elem_count == 0) {
                /* If the ring is empty, wait slightly longer. */
                gettimeofday(&tp, NULL); ts.tv_sec  = tp.tv_sec; ts.tv_nsec = (1000 * tp.tv_usec) + 122500000;   /* ~ 0.12 seconds. */
                continue;
            }
            dlog(6, "Checking ring...  (%d entries)\n", my->ring->elem_count);
        }
        pthread_mutex_lock(&my->commit_lock);  /* This will block until the current commit-list is cleared. */
        my->commit_count = 0;
        my->commit_list = SOS_ring_get_all(my->ring, &my->commit_count);
        int z;
        for (z = 0; z < my->commit_count; z++) {
            dlog(6, "  ... %ld\n", my->commit_list[z]);
        }
        pthread_mutex_unlock(&my->commit_lock);
        pthread_cond_signal(&my->commit_cond);
        gettimeofday(&tp, NULL); 
        ts.tv_sec  = tp.tv_sec; 
        ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;   /* ~ 0.06 seconds. */
    }
    pthread_mutex_unlock(&my->extract_lock);

    /* Free up the commit/inject thread to close down, too... */
    pthread_mutex_unlock(&my->commit_lock);
    pthread_cond_signal(&my->commit_cond);
    dlog(6, "Leaving thread safely.\n");
    pthread_exit(NULL);
}


void* SOSD_THREAD_pub_ring_storage_injector(void *args) {
    SOSD_pub_ring_mon *my = (SOSD_pub_ring_mon *) args;
    char func_name[SOS_DEFAULT_STRING_LEN] = {0};

    sprintf(func_name, "SOSD_THREAD_pub_ring_storage_injector(%s)", my->name);
    SOS_SET_CONTEXT(SOSD.sos_context, func_name);

    int       list_index;
    char      guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    SOS_pub  *pub;

    unsigned char     *buffer;
    unsigned char      buffer_static[SOS_DEFAULT_BUFFER_LEN];
    int       buffer_len;

    pthread_mutex_lock(&my->commit_lock);
    while (SOSD.daemon.running) {
        pthread_cond_wait(&my->commit_cond, &my->commit_lock);
        if (my->commit_count == 0) { continue; }

        if (my->commit_target == SOS_TARGET_LOCAL_SYNC) {
            SOSD_db_transaction_begin();
        }

        for (list_index = 0; list_index < my->commit_count; list_index++) {
            memset(guid_str, '\0', SOS_DEFAULT_STRING_LEN);
            sprintf(guid_str, "%ld", my->commit_list[list_index]);
            dlog(6, "Attempting to inject my->commit_list[%d] == pub(\"%s\")\n", list_index, guid_str);
            dlog(6, "Pulling up pub(%s) ...\n", guid_str);
            pub = SOSD.pub_table->get(SOSD.pub_table, guid_str);
            if (pub == NULL) { dlog(0, "ERROR!  SOSD.pub_table->get(SOSD_pub_table, \"%s\") == NULL     (skipping to next entry)\n", guid_str); continue; }

            switch (my->commit_target) {

            case SOS_TARGET_LOCAL_SYNC:
                if (pub->announced == SOSD_PUB_ANN_DIRTY) {
                    SOSD_db_insert_pub(pub);
                    SOSD_db_insert_data(pub);
                    pub->announced = SOSD_PUB_ANN_LOCAL;
                }
                if (SOS->role == SOS_ROLE_DB) {
                    SOSD_db_insert_vals(pub, my->val_intake, NULL);
                } else {
                    SOSD_db_insert_vals(pub, my->val_intake, my->val_outlet);
                    #if (SOSD_CLOUD_SYNC > 0)
                    SOS_ring_put( SOSD.cloud_sync->ring, my->commit_list[list_index] );
                    #endif
                }
                break;

            case SOS_TARGET_CLOUD_SYNC:
                #if (SOSD_CLOUD_SYNC > 0)
                if (SOS->role == SOS_ROLE_DB) { break; }
                if (pub->announced == SOSD_PUB_ANN_LOCAL) {
                    /* Announce to the CLOUD if it hasn't happened yet... */
                    buffer = buffer_static;
                    buffer_len = SOS_DEFAULT_BUFFER_LEN;
                    memset(buffer, '\0', buffer_len);
                    SOS_announce_to_buffer( pub, &buffer, &buffer_len );
                    SOSD_cloud_enqueue( buffer, buffer_len );
                    pub->announced = SOSD_PUB_ANN_CLOUD;
                }
                /* Push any enqueued VALUES for this pub out to the CLOUD */
                buffer     = buffer_static;
                buffer_len = SOS_DEFAULT_BUFFER_LEN;
                memset(buffer, '\0', buffer_len);
                SOS_val_snap_queue_to_buffer(my->val_intake, pub, &buffer, &buffer_len, true);
                if (buffer_len > 0) { SOSD_cloud_enqueue(buffer, buffer_len); }
                buffer_len = SOS_DEFAULT_BUFFER_LEN;
                memset(buffer, '\0', buffer_len);
                #endif
                break;

            default:
                dlog(0, "WARNING!  Attempting a storage injection into an unsupported target!  (%d)\n", my->commit_target);
            }

        }
        switch (my->commit_target) {
        case SOS_TARGET_LOCAL_SYNC: SOSD_db_transaction_commit(); break;
        case SOS_TARGET_CLOUD_SYNC:
            #if (SOSD_CLOUD_SYNC > 0)
            if ((SOS->role != SOS_ROLE_DB) && SOSD.daemon.running) {
                SOSD_cloud_fflush();
            }
            #endif
            break;
        default:
            dlog(5, "WARNING!  Your commit target doesn't make sense.  (%d)\n", my->commit_target);
            break;
        }
        free(my->commit_list);
        my->commit_count = 0;
    }
    pthread_mutex_unlock(&my->commit_lock);
    dlog(6, "Leaving thread safely.\n");
    pthread_exit(NULL);
}


/* -------------------------------------------------- */



void SOSD_handle_echo(unsigned char *msg, int msg_size) { 
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_echo");
    SOS_msg_header header;
    int ptr        = 0;
    int i          = 0;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_ECHO\n");

    ptr += SOS_buffer_unpack(SOS, msg, "iill",
                                    &header.msg_size,
                                    &header.msg_type,
                                    &header.msg_from,
                                    &header.pub_guid);

    i = send(SOSD.net.client_socket_fd, (void *) (msg + ptr), (msg_size - ptr), 0);
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        
    return;
}



void SOSD_handle_val_snaps(unsigned char *msg, int msg_size) {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_val_snaps");
    SOS_msg_header header;
    int ptr        = 0;
    int i          = 0;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_VAL_SNAPS\n");

    dlog(5, "Injecting snaps into local_sync queue...\n");
    SOS_val_snap_queue_from_buffer(SOSD.local_sync->val_intake, SOSD.pub_table, msg, msg_size);
    SOS_buffer_unpack(SOS, msg, "iill",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);

    if (header.pub_guid == 0) {
        dlog(1, "  ... ERROR: Being forced to insert a val_snap with a '0' pub_guid!  (msg_from == %ld)\n", header.msg_from);
    }
    
    SOS_ring_put(SOSD.local_sync->ring, header.pub_guid);

    dlog(5, "  ... done.\n");
        
    return;
}




void SOSD_handle_register(unsigned char *msg, int msg_size) {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_register");
    SOS_msg_header header;
    int  ptr             = 0;
    int  i               = 0;
    int  reply_len       = 0;
    long guid_block_from = 0;
    long guid_block_to   = 0;

    ptr += SOS_buffer_unpack(SOS, msg, "iill",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    dlog(5, "header.msg_type = SOS_MSG_TYPE_REGISTER\n");

    unsigned char reply[SOS_DEFAULT_REPLY_LEN] = {0};
    reply_len = 0;

    if (header.msg_from == 0) {
        /* A new client is registering with the daemon.
         * Supply them a block of GUIDs ...
         */
        SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &guid_block_from, &guid_block_to);
        memcpy(reply, &guid_block_from, sizeof(long));
        memcpy((reply + sizeof(long)), &guid_block_to, sizeof(long));
        reply_len = 2 * sizeof(long);

    } else {
        /* An existing client (such as sos_cmd) is coming back online,
         * don't give them any GUIDs.
         */
        SOSD_pack_ack(SOS, reply, &reply_len);
    }

    i = send( SOSD.net.client_socket_fd, (void *) reply, reply_len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
    }

    return;
}


void SOSD_handle_guid_block(unsigned char *msg, int msg_size) {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_register");
    SOS_msg_header header;
    long block_from   = 0;
    long block_to     = 0;
    int reply_len;
    int ptr;
    int i;

    ptr = 0;

    ptr += SOS_buffer_unpack(SOS, msg, "iill",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    dlog(5, "header.msg_type = SOS_MSG_TYPE_GUID_BLOCK\n");

    unsigned char reply[SOS_DEFAULT_REPLY_LEN] = {0};
    reply_len = 0;

    SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &block_from, &block_to);
    memcpy(reply, &block_from, sizeof(long));
    memcpy((reply + sizeof(long)), &block_to, sizeof(long));
    reply_len = 2 * sizeof(long);

    i = send( SOSD.net.client_socket_fd, (void *) reply, reply_len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
    }

    return;
}


void SOSD_handle_announce(unsigned char *msg, int msg_size) {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_announce");
    SOS_msg_header header;
    unsigned char  *ptr;
    unsigned char  reply[SOS_DEFAULT_REPLY_LEN] = {0};
    int   reply_len;
    int   buffer_pos;
    int   i;

    SOS_pub *pub;
    char     guid_str[SOS_DEFAULT_STRING_LEN] = {0};

    ptr        = msg;
    buffer_pos = 0;
    reply_len  = 0;


    /* Process the message into a pub handle... */

    ptr = 0;
    dlog(5, "header.msg_type = SOS_MSG_TYPE_ANNOUNCE\n");

    buffer_pos += SOS_buffer_unpack(SOS, msg, "iill",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    sprintf(guid_str, "%ld", header.pub_guid);

    /* Check the table for this pub ... */
    dlog(5, "  ... checking SOS->pub_table for GUID(%s):\n", guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, guid_str);
    if (pub == NULL) {
        dlog(5, "     ... NOPE!  Adding new pub to the table.\n");
        /* If it's not in the table, add it. */
        pub = SOS_pub_create(SOS, guid_str, SOS_NATURE_DEFAULT);
        SOSD.pub_table->put(SOSD.pub_table, guid_str, pub);
        pub->guid = header.pub_guid;
    } else {
        dlog(5, "     ... FOUND IT!\n");
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n", SOSD.pub_table->size(SOSD.pub_table));

    dlog(5, "Calling SOSD_apply_announce() ...\n");

    SOSD_apply_announce(pub, msg, msg_size);
    pub->announced = SOSD_PUB_ANN_DIRTY;

    if (SOS->role == SOS_ROLE_DB) { return; }

    dlog(5, "  ... pub(%ld)->elem_count = %d\n", pub->guid, pub->elem_count);

    SOSD_pack_ack(SOS, reply, &reply_len);
    
    i = send( SOSD.net.client_socket_fd, (void *) reply, reply_len, 0);
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }
    
    dlog(5, "  ... Done.\n");

    return;
}


void SOSD_handle_publish(unsigned char *msg, int msg_size)  {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_publish");
    SOS_msg_header header;
    long  guid = 0;
    int   ptr = 0;
    int   i   = 0;

    SOS_pub *pub;
    char     guid_str[SOS_DEFAULT_STRING_LEN] = {0};
    unsigned char     reply[SOS_DEFAULT_REPLY_LEN] = {0};
    int      reply_len;

    dlog(5, "header.msg_type = SOS_MSG_TYPE_PUBLISH\n");

    ptr += SOS_buffer_unpack(SOS, msg, "iill",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    sprintf(guid_str, "%ld", header.pub_guid);

    /* Check the table for this pub ... */
    dlog(5, "  ... checking SOS->pub_table for GUID(%s):\n", guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, guid_str);

    if (pub == NULL) {
        /* If it's not in the table, add it. */
        dlog(1, "     ... WHOAH!  PUBLISHING INTO A PUB NOT FOUND! (WEIRD!)  ADDING new pub to the table... (this is bogus, man)\n");
        pub = SOS_pub_create(SOS, guid_str, SOS_NATURE_DEFAULT);
        SOSD.pub_table->put(SOSD.pub_table, guid_str, pub);
        pub->guid = header.pub_guid;
    } else {
        dlog(5, "     ... FOUND it!\n");
    }
    dlog(5, "     ... SOSD.pub_table.size() = %d\n", SOSD.pub_table->size(SOSD.pub_table));


    SOSD_apply_publish( pub, msg, msg_size );

    dlog(5, "  ... inserting pub(%ld) into the 'to-do' ring queue. (It'll auto-announce as needed.)\n", pub->guid);
    SOS_ring_put(SOSD.local_sync->ring, pub->guid);

    if (SOSD_check_sync_saturation(SOSD.local_sync)) {
        pthread_cond_signal(&SOSD.local_sync->extract_cond);
    }

    dlog(5, "  ... done.   (SOSD.pub_ring->elem_count == %d)\n", SOSD.local_sync->ring->elem_count);

    if (SOS->role == SOS_ROLE_DB) { return; }

    SOSD_pack_ack(SOS, reply, &reply_len);
    
    i = send( SOSD.net.client_socket_fd, (void *) reply, reply_len, 0);
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else {
        dlog(5, "  ... send() returned the following bytecount: %d\n", i);
    }

    dlog(5, "  ... Done.\n");

    return;
}



void SOSD_handle_shutdown(unsigned char *msg, int msg_size) {

    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_shutdown");
    SOS_msg_header header;
    unsigned char reply[SOS_DEFAULT_REPLY_LEN] = {0};
    int reply_len = 0;
    int ptr = 0;
    int i   = 0;

    dlog(1, "header.msg_type = SOS_MSG_TYPE_SHUTDOWN\n");

    ptr += SOS_buffer_unpack(SOS, msg, "iill",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    if (SOS->role == SOS_ROLE_DAEMON) {
        SOSD_pack_ack(SOS, reply, &reply_len);
        
        i = send( SOSD.net.client_socket_fd, (void *) reply, reply_len, 0 );
        if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }
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

    return;
}



void SOSD_handle_check_in(unsigned char *msg, int msg_size) {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_handle_check_in");
    SOS_msg_header header;
    unsigned char feedback_msg[SOS_DEFAULT_FEEDBACK_LEN] = {0};
    unsigned char *ptr;
    unsigned char function_name[SOS_DEFAULT_STRING_LEN] = {0};
    int offset = 0;
    int i      = 0;

    dlog(1, "header.msg_type = SOS_MSG_TYPE_CHECK_IN\n");

    ptr += SOS_buffer_unpack(SOS, msg, "iill",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    if (SOS->role == SOS_ROLE_DAEMON) {
        /* Build a reply: */
        memset(&header, '\0', sizeof(SOS_msg_header));
        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_FEEDBACK;
        header.msg_from = 0;
        header.pub_guid = 0;

        ptr = feedback_msg;
        offset = 0;

        offset += SOS_buffer_pack(SOS, ptr, "iill",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);
        ptr = (feedback_msg + offset);

        /* TODO: { FEEDBACK } Currently this is a hard-coded 'exec function' case. */
        //memset(function_name, '\0', SOS_DEFAULT_STRING_LEN);
        snprintf(function_name, SOS_DEFAULT_STRING_LEN, "demo_function");

        offset += SOS_buffer_pack(SOS, ptr, "is",
            SOS_FEEDBACK_EXEC_FUNCTION,
            function_name);
        ptr = (feedback_msg + offset);

        /* Go back and set the message length to the actual length. */
        SOS_buffer_pack(SOS, feedback_msg, "i", offset);

        dlog(1, "Replying to CHECK_IN with SOS_FEEDBACK_EXEC_FUNCTION(%s)...\n", function_name);

        i = send( SOSD.net.client_socket_fd, (void *) feedback_msg, offset, 0 );
        if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
        else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }
    }

    dlog(5, "Done!\n");
    return;
}


void SOSD_handle_unknown(unsigned char *msg, int msg_size) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_handle_unknown");
    SOS_msg_header header;
    unsigned char reply[SOS_DEFAULT_REPLY_LEN] = {0};
    int reply_len = 0;
    int ptr = 0;
    int i   = 0;

    dlog(1, "header.msg_type = UNKNOWN\n");

    ptr += SOS_buffer_unpack(SOS, msg, "iill",
                             &header.msg_size,
                             &header.msg_type,
                             &header.msg_from,
                             &header.pub_guid);

    dlog(1, "header.msg_size == %d\n", header.msg_size);
    dlog(1, "header.msg_type == %d\n", header.msg_type);
    dlog(1, "header.msg_from == %ld\n", header.msg_from);
    dlog(1, "header.pub_guid == %ld\n", header.pub_guid);

    if (SOS->role == SOS_ROLE_DB) { return; }

    SOSD_pack_ack(SOS, reply, &reply_len);

    i = send( SOSD.net.client_socket_fd, (void *) reply, reply_len, 0 );
    if (i == -1) { dlog(0, "Error sending a response.  (%s)\n", strerror(errno)); }
    else { dlog(5, "  ... send() returned the following bytecount: %d\n", i); }

    return;
}



void SOSD_setup_socket() {
    SOS_SET_CONTEXT(SOSD.sos_context, "daemon_setup_socket");
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
    case SOS_ROLE_DAEMON:  snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME /* ".mon" */); break;
    case SOS_ROLE_DB:      snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME /* ".dat" */); break;
    case SOS_ROLE_CONTROL: snprintf(SOSD.daemon.name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME /* ".ctl" */); break;
    default: break;
    }

    /* [lock file]
     *     create and hold lock file to prevent multiple daemon spawn
     */
    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.daemon.lock_file, SOS_DEFAULT_STRING_LEN, "%s/%s.%d.lock", SOSD.daemon.work_dir, SOSD.daemon.name, SOS->config.comm_rank);
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
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN, "%s.%d.log", SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.daemon.log_file, SOS_DEFAULT_STRING_LEN, "%s.local.log", SOSD.daemon.name);
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
        long guid_block_size = (long) ((long double) SOS_DEFAULT_UID_MAX / (long double) SOS->config.comm_size);
        long guid_my_first   = (long) SOS->config.comm_rank * guid_block_size;
        SOS_uid_init(SOS, &SOSD.guid, guid_my_first, (guid_my_first + (guid_block_size - 1)));
    #else
        dlog(1, "DATA NOTE:  Running in local mode, CLOUD_SYNC is disabled.\n");
        dlog(1, "DATA NOTE:  GUID values are unique only to this node.\n");
        SOS_uid_init(&SOSD.guid, 1, SOS_DEFAULT_UID_MAX);
    #endif
    dlog(1, "  ... (%ld ---> %ld)\n", SOSD.guid->next, SOSD.guid->last);

    /* [hashtable]
     *    storage system for received pubs.  (will enque their key -> db)
     */
    dlog(1, "Setting up a hash table for pubs...\n");
    SOSD.pub_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(1, "Daemon initialization is complete.\n");
    SOSD.daemon.running = 1;
    return;
}

void SOSD_claim_guid_block(SOS_uid *id, int size, long *pool_from, long *pool_to) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_guid_claim_range");

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
    }

    pthread_mutex_unlock( id->lock );

    return;
}



void SOSD_apply_announce( SOS_pub *pub, unsigned char *msg, int msg_len ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_announce");

    dlog(6, "Calling SOS_announce_from_buffer()...\n");
    SOS_announce_from_buffer(pub, msg);

    return;
}


void SOSD_apply_publish( SOS_pub *pub, unsigned char *msg, int msg_len ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_apply_publish");

    dlog(6, "Calling SOS_publish_from_buffer()...\n");
    SOS_publish_from_buffer(pub, msg, SOS->task.val_intake);

    return;
}





void SOSD_display_logo(void) {
    int choice = 0;

    srand(getpid());
    choice = rand() % 6;

    printf("--------------------------------------------------------------------------------\n");
    printf("\n");

    switch (choice) {
    case 0:
        printf("               _/_/_/    _/_/      _/_/_/    ))) ))  )    Scalable\n");
        printf("            _/        _/    _/  _/          ((( ((  (     Observation\n");
        printf("             _/_/    _/    _/    _/_/        ))) ))  )    System\n");
        printf("                _/  _/    _/        _/      ((( ((  (     for Scientific\n");
        printf("         _/_/_/      _/_/    _/_/_/          ))) ))  )    Workflows\n");
        break;

    case 1:
        printf("     @@@@@@    @@@@@@    @@@@@@   @@@@@@@@  @@@        @@@@@@   @@@  @@@  @@@\n");
        printf("    @@@@@@@   @@@@@@@@  @@@@@@@   @@@@@@@@  @@@       @@@@@@@@  @@@  @@@  @@@\n");
        printf("    !@@       @@!  @@@  !@@       @@!       @@!       @@!  @@@  @@!  @@!  @@!\n");
        printf("    !@!       !@!  @!@  !@!       !@!       !@!       !@!  @!@  !@!  !@!  !@!\n");
        printf("    !!@@!!    @!@  !@!  !!@@!!    @!!!:!    @!!       @!@  !@!  @!!  !!@  @!@\n");
        printf("     !!@!!!   !@!  !!!   !!@!!!   !!!!!:    !!!       !@!  !!!  !@!  !!!  !@!\n");
        printf("         !:!  !!:  !!!       !:!  !!:       !!:       !!:  !!!  !!:  !!:  !!:\n");
        printf("        !:!   :!:  !:!      !:!   :!:        :!:      :!:  !:!  :!:  :!:  :!:\n");
        printf("    :::: ::   ::::: ::  :::: ::    ::        :: ::::  ::::: ::   :::: :: ::: \n");
        printf("    :: : :     : :  :   :: : :     :        : :: : :   : :  :     :: :  : :  \n");
        printf("\n");
        printf("       |                                                                |    \n");
        printf("    -- + --   Scalable Observation System for Scientific Workflows   -- + -- \n");
        printf("       |                                                                |    \n");
        break;

    case 2:
        printf("      {__ __      {____       {__ __      {__ {__                      \n");
        printf("    {__    {__  {__    {__  {__    {__  {_    {__                      \n");
        printf("     {__      {__        {__ {__      {_{_ {_ {__   {__    {__     {___\n");
        printf("       {__    {__        {__   {__      {__   {__ {__  {__  {__  _  {__\n");
        printf("          {__ {__        {__      {__   {__   {__{__    {__ {__ {_  {__\n");
        printf("    {__    {__  {__     {__ {__    {__  {__   {__ {__  {__  {_ {_ {_{__\n");
        printf("      {__ __      {____       {__ __    {__  {___   {__    {___    {___\n");
        printf("\n");
        printf("     [...Scalable..Observation..System..for..Scientific..Workflows...]\n");
        break;

    case 3:
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


    case 4:
        printf("_._____. .____  .____.._____. _  ..\n");
        printf("__  ___/_  __ \\_  ___/__  __/__  /________      __    .:|   Scalable\n");
        printf(".____ \\_  / / /.___ \\__  /_ ._  /_  __ \\_ | /| / /    .:|   Observation\n");
        printf("____/ // /_/ /.___/ /_  __/ _  / / /_/ /_ |/ |/ /     .:|   System for Scientific\n");
        printf("/____/ \\____/ /____/ /_/    /_/  \\____/____/|__/      .:|   Workflows\n");
        break;

    case 5:
        printf("     O)) O)      O))))       O)) O)      O)) O))                      \n");
        printf("   O))    O))  O))    O))  O))    O))  O)    O))                      \n");
        printf("    O))      O))        O)) O))      O)O) O) O))   O))    O))     O)))\n");
        printf("      O))    O))        O))   O))      O))   O)) O))  O))  O))     O))\n");
        printf("         O)) O))        O))      O))   O))   O))O))    O)) O)) O)  O))\n");
        printf("   O))    O))  O))     O)) O))    O))  O))   O)) O))  O))  O) O) O)O))\n");
        printf("     O)) O)      O))))       O)) O)    O))  O)))   O))    O)))    O)))\n");
        printf("\n");
        printf("       +------------------------------------------------------+\n");
        printf("       | Scalable Observation System for Scientific Workflows |\n");
        printf("       +------------------------------------------------------+\n");
        printf("\n");
        break;
    }

    printf("\n");
    printf("   Version: %s\n", SOS_VERSION);
    printf("   Builder: %s\n", SOS_BUILDER);
    printf("\n");
    printf("--------------------------------------------------------------------------------\n");

    return;
}
