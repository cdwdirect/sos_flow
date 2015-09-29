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

#define USAGE          "usage:   $ sosd --port <number> --buffer_len <bytes> --listen_backlog <len> [--work_dir <path>]"

int main(int argc, char *argv[])  {
    int elem, next_elem;
    int retval;

    SOSD.work_dir    = (char *) &SOSD_DEFAULT_DIR;
    SOSD.daemon_name = (char *) malloc(SOS_DEFAULT_STRING_LEN);
    SOSD.lock_file   = (char *) malloc(SOS_DEFAULT_STRING_LEN);
    SOSD.log_file    = (char *) malloc(SOS_DEFAULT_STRING_LEN);
    SOSD.db_file     = (char *) malloc(SOS_DEFAULT_STRING_LEN);

    memset(SOSD.daemon_name, '\0', SOS_DEFAULT_STRING_LEN);
    memset(SOSD.lock_file, '\0', SOS_DEFAULT_STRING_LEN);
    memset(SOSD.log_file, '\0', SOS_DEFAULT_STRING_LEN);
    memset(SOSD.db_file, '\0', SOS_DEFAULT_STRING_LEN);
    SOSD.db_ready = 0;

    SOS.role = SOS_ROLE_DAEMON;

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
        else if ( strcmp(argv[elem], "--work_dir"        ) == 0) { SOSD.work_dir           = argv[next_elem];       } /* optional */
        else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    SOSD.net.port_number = atoi(SOSD.net.server_port);
    if ( (SOSD.net.port_number < 1)
         || (SOSD.net.buffer_len < 1)
         || (SOSD.net.listen_backlog < 1) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }

    memset(&SOSD.daemon_pid_str, '\0', 256);

    #ifdef SOSD_CLOUD_SYNC
    SOSD_cloud_init( &argc, &argv );
    #endif

    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[daemon.%d.main]: Calling SOSD_init()...\n", SOS.config.comm_rank); fflush(stdout); }
    SOSD_init();
    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[daemon.%d.main]: Calling SOS_init...\n", SOS.config.comm_rank);  fflush(stdout); }
    SOS_init( &argc, &argv, SOS.role );
    SOS_SET_WHOAMI(whoami, "main");
    dlog(0, "[%s]: Calling register_signal_handler()...\n", whoami);
    if (SOS_DEBUG) SOS_register_signal_handler();
    dlog(0, "[%s]: Calling daemon_setup_socket()...\n", whoami);
    SOSD_setup_socket();
    dlog(0, "[%s]: Calling daemon_init_database()...\n", whoami);
    SOSD_init_database();
    dlog(0, "[%s]: Creating ring queue monitors to track 'to-do' list for pubs...\n", whoami);
    SOSD_pub_ring_monitor_init(&SOSD.local_sync, "local_sync", NULL, SOS_ROLE_DAEMON);
    SOSD_pub_ring_monitor_init(&SOSD.cloud_sync, "cloud_sync", NULL, SOS_ROLE_DB);

    /* Go! */
    dlog(0, "[%s]: Calling daemon_listen_loop()...\n", whoami);
    SOSD_listen_loop();
  

    /* Done!  Cleanup and shut down. */
    dlog(0, "[%s]: Ending the pub_ring monitors:\n", whoami);
    dlog(0, "[%s]:   ... waiting for the pub_ring monitor to iterate and exit.\n", whoami);
    pthread_join( *(SOSD.local_sync->extract_t), NULL);
    pthread_join( *(SOSD.local_sync->commit_t), NULL);
    pthread_join( *(SOSD.cloud_sync->extract_t), NULL);
    pthread_join( *(SOSD.cloud_sync->commit_t), NULL);
    dlog(0, "[%s]:   ... destroying the ring monitors...\n", whoami);
    SOSD_pub_ring_monitor_destroy(SOSD.local_sync);
    SOSD_pub_ring_monitor_destroy(SOSD.cloud_sync);
    dlog(0, "[%s]:   ... done.\n", whoami);
    dlog(0, "[%s]: Closing the database.\n", whoami);
    SOSD_db_close_database();
    dlog(0, "[%s]: Shutting down SOS services.\n", whoami);
    SOS_uid_destroy( SOSD.guid );
    #ifdef SOSD_CLOUD_SYNC
    SOSD_cloud_finalize();
    #endif
    SOS_finalize();
    dlog(0, "[%s]: Closing the socket.\n", whoami);
    shutdown(SOSD.net.server_socket_fd, SHUT_RDWR);
    dlog(0, "[%s]: Exiting daemon's main() gracefully.\n", whoami);

    if (SOSD_DAEMON_LOG) { fclose(sos_daemon_log_fptr); }

    close(sos_daemon_lock_fptr);
    remove(SOSD.lock_file);

    free(SOSD.daemon_name);
    free(SOSD.lock_file);
    free(SOSD.log_file);
    free(SOSD.db_file);
    
    return(EXIT_SUCCESS);
} //end: main()


void SOSD_pub_ring_monitor_init(SOSD_pub_ring_mon **mon_var, char *name_var, SOS_ring_queue *ring_var, SOS_role target_var) {
    SOS_SET_WHOAMI(whoami, "SOSD_pub_ring_monitor_init");
    SOSD_pub_ring_mon *mon;
    int retval;

    mon = *mon_var = malloc(sizeof(SOSD_pub_ring_mon));
    memset(mon, '\0', sizeof(SOSD_pub_ring_mon));

    mon->name = name_var;
    if (ring_var == NULL) {
        SOS_ring_init(&mon->ring);
    } else {
        mon->ring = ring_var;
    }
    mon->extract_t     = (pthread_t *) malloc(sizeof(pthread_t));
    mon->extract_cond  = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    mon->extract_lock  = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    mon->commit_t      = (pthread_t *) malloc(sizeof(pthread_t));
    mon->commit_cond   = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    mon->commit_lock   = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    mon->commit_list   = NULL;
    mon->commit_count  = 0;
    mon->commit_target = target_var;

    retval = pthread_cond_init(mon->extract_cond, NULL); 
    if (retval != 0) { dlog(0, "[%s]: ERROR!  Could not initialize the mon(%s)->extract_cond pthread_cond_t variable.  (%s)\n", whoami, mon->name, strerror(errno)); exit(EXIT_FAILURE); } 
    retval = pthread_cond_init(mon->commit_cond, NULL);
    if (retval != 0) { dlog(0, "[%s]: ERROR!  Could not initialize the mon(%s)->commit_cond pthread_cond_t variable.  (%s)\n", whoami, mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_mutex_init(mon->extract_lock, NULL);
    if (retval != 0) { dlog(0, "[%s]: ERROR!  Could not initialize the mon(%s)->extract_lock mutex.  (%s)\n", whoami, mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_mutex_init(mon->commit_lock, NULL);
    if (retval != 0) { dlog(0, "[%s]: ERROR!  Could not initialize the mon(%s)->commit_lock mutex.  (%s)\n", whoami, mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_create( mon->extract_t, NULL, (void *) SOSD_THREAD_pub_ring_list_extractor, mon );
    if (retval != 0) { dlog(0, "[%s]: ERROR!  Could not initialize the mon(%s)->extract_t thread.  (%s)\n", whoami, mon->name, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_create( mon->commit_t, NULL, (void *) SOSD_THREAD_pub_ring_storage_injector, mon );
    if (retval != 0) { dlog(0, "[%s]: ERROR!  Could not initialize the mon(%s)->commit_t thread.  (%s)\n", whoami, mon->name, strerror(errno)); exit(EXIT_FAILURE); }

    return;
}

void SOSD_pub_ring_monitor_destroy(SOSD_pub_ring_mon *mon) {
    SOS_SET_WHOAMI(whoami, "SOSD_pub_ring_monitor_destroy");

    pthread_mutex_destroy( mon->extract_lock );
    pthread_mutex_destroy( mon->commit_lock );
    pthread_cond_destroy( mon->extract_cond );
    pthread_cond_destroy( mon->commit_cond );

    free(mon->extract_lock);
    free(mon->extract_cond);
    free(mon->extract_t);
    free(mon->commit_lock);
    free(mon->commit_cond);
    free(mon->commit_t);
    SOS_ring_destroy(mon->ring);
    free(mon);

    return;
}



/* -------------------------------------------------- */
void SOSD_listen_loop() {
    SOS_SET_WHOAMI(whoami, "daemon_listen_loop");
    SOS_msg_header header;
    int      i, byte_count;

    char    *buffer;
    buffer = (char *) malloc(sizeof(char) * SOSD.net.buffer_len);

    dlog(0, "[%s]: Entering main loop...\n", whoami);
    while (SOSD.daemon_running) {
        memset(buffer, '\0', SOSD.net.buffer_len);
        memset(&header, '\0', sizeof(SOS_msg_header));
        byte_count = 0;

        dlog(5, "[%s]: Listening for a message...\n", whoami);
        SOSD.net.peer_addr_len = sizeof(SOSD.net.peer_addr);
        SOSD.net.client_socket_fd = accept(SOSD.net.server_socket_fd, (struct sockaddr *) &SOSD.net.peer_addr, &SOSD.net.peer_addr_len);
        i = getnameinfo((struct sockaddr *) &SOSD.net.peer_addr, SOSD.net.peer_addr_len, SOSD.net.client_host, NI_MAXHOST, SOSD.net.client_port, NI_MAXSERV, NI_NUMERICSERV);
        if (i != 0) { dlog(0, "[%s]: Error calling getnameinfo() on client connection.  (%s)\n", whoami, strerror(errno)); break; }

        byte_count = recv(SOSD.net.client_socket_fd, (void *) buffer, SOSD.net.buffer_len, 0);
        if (byte_count < 1) {
            dlog(1, "[%s]:   ... recv() call returned an errror.  (%s)\n", whoami, strerror(errno));
            continue;
        }

        if (byte_count >= sizeof(SOS_msg_header)) {
            memcpy(&header, buffer, sizeof(SOS_msg_header));
            dlog(6, "[%s]:   ... Received %d of %d bytes in this message.\n", whoami, byte_count, header.msg_size);
        } else {
            dlog(0, "[%s]:   ... Received short (useless) message.\n", whoami);  continue;
        }

        while (byte_count < header.msg_size) {
            byte_count += recv(SOSD.net.client_socket_fd, (void *) (buffer + byte_count), SOSD.net.buffer_len, 0);
            dlog(6, "[%s]:      ... %d of %d ...\n", whoami, byte_count, header.msg_size);
        }

        dlog(5, "[%s]: Received connection.\n", whoami);
        dlog(5, "[%s]:   ... byte_count = %d\n", whoami, byte_count);
        dlog(5, "[%s]:   ... msg_from = %ld\n", whoami, header.msg_from);

        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER: dlog(5, "[%s]:   ... msg_type = REGISTER (%d)\n", whoami, header.msg_type);
            SOSD_handle_register(buffer, byte_count); break; 
        case SOS_MSG_TYPE_GUID_BLOCK: dlog(5, "[%s]:   ... msg_type = GUID_BLOCK (%d)\n", whoami, header.msg_type);
            SOSD_handle_guid_block(buffer, byte_count); break;
        case SOS_MSG_TYPE_ANNOUNCE: dlog(5, "[%s]:   ... msg_type = ANNOUNCE (%d)\n", whoami, header.msg_type);
            SOSD_handle_announce(buffer, byte_count); break;
        case SOS_MSG_TYPE_PUBLISH:  dlog(5, "[%s]:   ... msg_type = PUBLISH (%d)\n", whoami, header.msg_type);
            SOSD_handle_publish(buffer, byte_count); break;
        case SOS_MSG_TYPE_ECHO:     dlog(5, "[%s]:   ... msg_type = ECHO (%d)\n", whoami, header.msg_type);
            SOSD_handle_echo(buffer, byte_count); break;
        case SOS_MSG_TYPE_SHUTDOWN: dlog(5, "[%s]:   ... msg_type = SHUTDOWN (%d)\n", whoami, header.msg_type);
            SOSD_handle_shutdown(buffer, byte_count); break;
        default:                    dlog(1, "[%s]:   ... msg_type = UNKNOWN (%d)\n", whoami, header.msg_type); break;
            SOSD_handle_unknown(buffer, byte_count); break;
        }
        close( SOSD.net.client_socket_fd );
    }
    free(buffer);
    dlog(1, "[%s]: Leaving the socket listening loop.\n", whoami);

    return;
}

/* -------------------------------------------------- */


void* SOSD_THREAD_pub_ring_list_extractor(void *args) {
    SOSD_pub_ring_mon *my = (SOSD_pub_ring_mon *) args;
    char func_name[SOS_DEFAULT_STRING_LEN];
    memset(func_name, '\0', SOS_DEFAULT_STRING_LEN);
    sprintf(func_name, "SOSD_THREAD_pub_ring_extractor(%s)", my->name);
    SOS_SET_WHOAMI(whoami, func_name);

    struct timespec ts;
    struct timeval  tp;
    int wake_type;

    pthread_mutex_lock(my->extract_lock);
    while (SOSD.daemon_running) {
        gettimeofday(&tp, NULL); ts.tv_sec  = 0 + tp.tv_sec; ts.tv_nsec = 200 + (1000 * tp.tv_usec);
        wake_type = pthread_cond_timedwait(my->extract_cond, my->extract_lock, &ts);
        if (wake_type == ETIMEDOUT) {
            /* ...any special actions that need to happen if timed-out vs. called-explicitly */
            if (my->ring->elem_count == 0) continue;
            dlog(6, "[%s]: Checking ring...  (%d entries)\n", whoami, my->ring->elem_count);
        }
        pthread_mutex_lock(my->commit_lock);  /* This will block until the current commit-list is cleared. */
        my->commit_count = 0;
        my->commit_list = SOS_ring_get_all(my->ring, &my->commit_count);
        pthread_mutex_unlock(my->commit_lock);
        pthread_cond_signal(my->commit_cond);
    }
    pthread_mutex_unlock(my->extract_lock);
    dlog(0, "[%s]: Leaving thread safely.\n", whoami);
    pthread_exit(NULL);
}


void* SOSD_THREAD_pub_ring_storage_injector(void *args) {
    SOSD_pub_ring_mon *my = (SOSD_pub_ring_mon *) args;
    char func_name[SOS_DEFAULT_STRING_LEN];
    memset(func_name, '\0', SOS_DEFAULT_STRING_LEN);
    sprintf(func_name, "SOSD_THREAD_pub_ring_storage_injector(%s)", my->name);
    SOS_SET_WHOAMI(whoami, func_name);

    int       list_index;
    char      guid_str[SOS_DEFAULT_STRING_LEN];
    SOS_pub  *pub;

    char     *buffer;
    char      buffer_static[SOS_DEFAULT_BUFFER_LEN];
    int       buffer_len;

    pthread_mutex_lock(my->commit_lock);
    while (SOSD.daemon_running) {
        pthread_cond_wait(my->commit_cond, my->commit_lock);

        for (list_index = 0; list_index < my->commit_count; list_index++) {
            memset(guid_str, '\0', SOS_DEFAULT_STRING_LEN);
            sprintf(guid_str, "%ld", my->commit_list[list_index]);
            dlog(6, "[%s]: Attempting to inject my->commit_list[%d] == pub(\"%s\")\n", whoami, list_index, guid_str);
            dlog(6, "[%s]: Pulling up pub(%s) ...\n", whoami, guid_str);
            pub = SOSD.pub_table->get(SOSD.pub_table, guid_str);
            if (pub == NULL) { dlog(0, "[%s]: ERROR!  SOSD.pub_table->get(SOSD_pub_table, \"%s\") == NULL     (skipping to next entry)\n", whoami, guid_str); continue; }

            /* TODO:{ STORAGE_INJECT } Make this more performant by grouping transactions. */

            switch (my->commit_target) {
            case SOS_ROLE_DAEMON:
                if (pub->announced != SOSD_PUB_ANN_LOCAL) {
                    SOSD_db_insert_pub(pub);
                    pub->announced = SOSD_PUB_ANN_LOCAL;
                }
                SOSD_db_insert_data(pub);
                SOS_ring_put( SOSD.cloud_sync->ring, my->commit_list[list_index] );
                break;

            case SOS_ROLE_DB:
                if (pub->announced != SOSD_PUB_ANN_CLOUD) {
                    dlog(1, "[%s]: DAEMON ---ANNOUNCE---> 'SOS CLOUD'      (to-do...)\n", whoami);
                    /* Announce to the database... */
                    buffer = buffer_static;
                    buffer_len = SOS_DEFAULT_BUFFER_LEN;
                    memset(buffer, '\0', buffer_len);
                    SOS_announce_to_string( pub, &buffer, &buffer_len );
                    SOSD_cloud_send( buffer, buffer_len );
                    pub->announced = SOSD_PUB_ANN_CLOUD;
                    if (buffer_len > SOS_DEFAULT_BUFFER_LEN) {
                        /* The serialization function had to dynamically allocate storage. */
                        free(buffer);
                        buffer = buffer_static;
                    }
                }
                dlog(1, "[%s]: DAEMON ----PUBLISH---> 'SOS CLOUD'      (to-do...)\n", whoami);
                buffer     = buffer_static;
                buffer_len = SOS_DEFAULT_BUFFER_LEN;
                memset(buffer, '\0', buffer_len);
                SOS_publish_to_string( pub, &buffer, &buffer_len );
                SOSD_cloud_send( buffer, buffer_len );
                if (buffer_len > SOS_DEFAULT_BUFFER_LEN) {
                    /* The serialization function had to dynamically allocates storage. */
                    free(buffer);
                    buffer = buffer_static;
                }
                break;

            default:
                dlog(0, "[%s]: WARNING!  Attempting a storage injection into an unsupported target!  (%d)\n", whoami, my->commit_target);
            }
        }
        free(my->commit_list);
    }
    pthread_mutex_unlock(my->commit_lock);
    dlog(0, "[%s]: Leaving thread safely.\n", whoami);
    pthread_exit(NULL);
}


/* -------------------------------------------------- */



void SOSD_handle_echo(char *msg, int msg_size) { 
    SOS_SET_WHOAMI(whoami, "daemon_handle_echo");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_ECHO\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    i = send(SOSD.net.client_socket_fd, (void *) (msg + ptr), (msg_size - sizeof(SOS_msg_header)), 0);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
        
    return;
}



void SOSD_handle_register(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_register");
    SOS_msg_header header;
    int  ptr             = 0;
    int  i               = 0;
    int  reply_len       = 0;
    long guid_block_from = 0;
    long guid_block_to   = 0;

    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);
    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_REGISTER\n", whoami);

    char response[SOS_DEFAULT_ACK_LEN];
    memset(response, '\0', SOS_DEFAULT_ACK_LEN);
    reply_len = 0;

    if (header.msg_from == 0) {
        /* A new client is registering with the daemon.
         * Supply them a block of GUIDs ...
         */
        SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &guid_block_from, &guid_block_to);
        memcpy(response, &guid_block_from, sizeof(long));
        memcpy((response + sizeof(long)), &guid_block_to, sizeof(long));
        reply_len = 2 * sizeof(long);

    } else {
        /* An existing client (such as sos_cmd) is coming back online,
         * don't give them any GUIDs.
         */
        sprintf(response, "ACK");
        reply_len = 4;
    }

    i = send( SOSD.net.client_socket_fd, (void *) response, reply_len, 0 );
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else {
        dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i);
    }

    return;
}


void SOSD_handle_guid_block(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_register");
    SOS_msg_header header;
    long block_from   = 0;
    long block_to     = 0;
    int reply_len;
    int ptr;
    int i;

    ptr = 0;
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);
    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_GUID_BLOCK\n", whoami);

    char response[SOS_DEFAULT_ACK_LEN];
    memset(response, '\0', SOS_DEFAULT_ACK_LEN);
    reply_len = 0;

    SOSD_claim_guid_block(SOSD.guid, SOS_DEFAULT_GUID_BLOCK, &block_from, &block_to);
    memcpy(response, &block_from, sizeof(long));
    memcpy((response + sizeof(long)), &block_to, sizeof(long));
    reply_len = 2 * sizeof(long);

    i = send( SOSD.net.client_socket_fd, (void *) response, reply_len, 0 );
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else {
        dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i);
    }

    return;
}


void SOSD_handle_announce(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_announce");
    SOS_msg_header header;
    int   ptr;
    int   i;
    char *response;
    int   response_len;
    char  response_stack[SOS_DEFAULT_BUFFER_LEN];
    char  response_alloc;

    SOS_pub *pub;
    char     guid_str[SOS_DEFAULT_STRING_LEN];

    response = response_stack;
    response_alloc = 0;
    response_len = 0;

    /* Process the message into a pub handle... */

    ptr = 0;
    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_ANNOUNCE\n", whoami);

    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);
    memset(guid_str, '\0', SOS_DEFAULT_STRING_LEN);
    sprintf(guid_str, "%ld", header.pub_guid);

    /* Check the table for this pub ... */
    dlog(5, "[%s]:    ... checking SOS.pub_table for GUID(%s) --> ", whoami, guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, guid_str);
    if (pub == NULL) {
        dlog(5, "NOPE!  Adding new pub to the table.\n");
        /* If it's not in the table, add it. */
        pub = SOS_new_pub(guid_str);
        SOSD.pub_table->put(SOSD.pub_table, guid_str, pub);
        pub->guid = header.pub_guid;
    } else {
        dlog(5, "FOUND IT!\n");
    }

    dlog(5, "[%s]: calling SOSD_apply_announce() ...\n", whoami);

    SOSD_apply_announce(pub, msg, msg_size);
    pub->announced = SOSD_PUB_ANN_DIRTY;

    dlog(5, "[%s]:   ... pub(%ld)->elem_count = %d\n", whoami, pub->guid, pub->elem_count);

    memset(response, '\0', SOS_DEFAULT_ACK_LEN);
    sprintf(response, "ACK");
    response_len = 4;

    if (response_len > SOS_DEFAULT_ACK_LEN) {
        response = (char *) malloc( response_len );
        if (response == NULL) { dlog(0, "[%s]: ERROR!  Could not allocate memory for an announcement response!  (%s)\n", whoami, strerror(errno));  exit(1); }
        memset (response, '\0', response_len);
        response_alloc = 1;
    }

    i = send( SOSD.net.client_socket_fd, (void *) response, response_len, 0);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else { dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i); }
    
    dlog(5, "[%s]:   ... Done.\n", whoami);

    return;
}


void SOSD_handle_publish(char *msg, int msg_size)  {
    SOS_SET_WHOAMI(whoami, "daemon_handle_publish");
    SOS_msg_header header;
    long  guid = 0;
    int   ptr = 0;
    int   i   = 0;

    SOS_pub *pub;
    char     guid_str[SOS_DEFAULT_STRING_LEN];
    char     response[SOS_DEFAULT_ACK_LEN];
    int      response_len;

    memset(guid_str, '\0', SOS_DEFAULT_STRING_LEN);

    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_PUBLISH\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    sprintf(guid_str, "%ld", header.pub_guid);
    /* Check the table for this pub ... */
    dlog(5, "[%s]:   ... checking SOS.pub_table for GUID(%s) --> ", whoami, guid_str);
    pub = (SOS_pub *) SOSD.pub_table->get(SOSD.pub_table, guid_str);
    if (pub == NULL) {
        /* If it's not in the table, add it. */
        dlog(1, "WHOAH!  PUBLISHING INTO A PUB NOT FOUND! (WEIRD!)  ADDING new pub to the table... (this is bogus, man)\n");
        pub = SOS_new_pub(guid_str);
        SOSD.pub_table->put(SOSD.pub_table, guid_str, pub);
        pub->guid = header.pub_guid;
    } else {
        dlog(5, "FOUND it!\n");
    }

    SOSD_apply_publish( pub, msg, msg_size );

    dlog(5, "[%s]:   ... inserting pub(%ld) into the 'to-do' ring queue. (It'll auto-announce as needed.)\n", whoami, pub->guid);
    SOS_ring_put(SOSD.local_sync->ring, pub->guid);

    if (SOSD_check_sync_saturation(SOSD.local_sync)) {
        pthread_cond_signal(SOSD.local_sync->extract_cond);
    }

    dlog(5, "[%s]:   ... done.   (SOSD.pub_ring->elem_count == %d)\n", whoami, SOSD.local_sync->ring->elem_count);

    memset (response, '\0', SOS_DEFAULT_ACK_LEN);
    sprintf(response, "ACK");
    response_len = 4;

    i = send( SOSD.net.client_socket_fd, (void *) response, response_len, 0);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else {
        dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i);
    }

    dlog(5, "[%s]:   ... Done.\n", whoami);

    return;
}



void SOSD_handle_shutdown(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_shutdown");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(1, "[%s]: header.msg_type = SOS_MSG_TYPE_SHUTDOWN\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    char response[SOS_DEFAULT_BUFFER_LEN];
    memset ( response, '\0', SOS_DEFAULT_BUFFER_LEN );
    sprintf( response, "I received your SHUTDOWN!");

    i = send( SOSD.net.client_socket_fd, (void *) response, strlen(response), 0 );
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else { dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i); }

    SOSD.daemon_running = 0;

    return;
}



void SOSD_handle_unknown(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_unknown");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(1, "[%s]: header.msg_type = UNKNOWN\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    char response[SOS_DEFAULT_BUFFER_LEN];
    memset ( response, '\0', SOS_DEFAULT_BUFFER_LEN );
    sprintf( response, "SOS daemon did not understand your message!");

    i = send( SOSD.net.client_socket_fd, (void *) response, strlen(response), 0 );
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else { dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i); }

    return;
}



void SOSD_setup_socket() {
    SOS_SET_WHOAMI(whoami, "daemon_setup_socket");
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
    if (i != 0) { dlog(0, "[%s]: Error!  getaddrinfo() failed. (%s) Exiting daemon.\n", whoami, strerror(errno)); exit(EXIT_FAILURE); }

    for ( SOSD.net.server_addr = SOSD.net.result ; SOSD.net.server_addr != NULL ; SOSD.net.server_addr = SOSD.net.server_addr->ai_next ) {
        dlog(1, "[%s]: Trying an address...\n", whoami);

        SOSD.net.server_socket_fd = socket(SOSD.net.server_addr->ai_family, SOSD.net.server_addr->ai_socktype, SOSD.net.server_addr->ai_protocol );
        if ( SOSD.net.server_socket_fd < 1) {
            dlog(0, "[%s]:   ... failed to get a socket.  (%s)\n", whoami, strerror(errno));
            continue;
        }

        /*
         *  Allow this socket to be reused/rebound quickly by the daemon.
         */
        if ( setsockopt( SOSD.net.server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            dlog(0, "[%s]:   ... could not set socket options.  (%s)\n", whoami, strerror(errno));
            continue;
        }

        if ( bind( SOSD.net.server_socket_fd, SOSD.net.server_addr->ai_addr, SOSD.net.server_addr->ai_addrlen ) == -1 ) {
            dlog(0, "[%s]:   ... failed to bind to socket.  (%s)\n", whoami, strerror(errno));
            close( SOSD.net.server_socket_fd );
            continue;
        } 
        /* If we get here, we're good to stop looking. */
        break;
    }

    if ( SOSD.net.server_socket_fd < 0 ) {
        dlog(0, "[%s]:   ... could not socket/setsockopt/bind to anything in the result set.  last errno = (%d:%s)\n", whoami, errno, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        dlog(0, "[%s]:   ... got a socket, and bound to it!\n", whoami);
    }

    freeaddrinfo(SOSD.net.result);

    /*
     *   Enforce that this is a BLOCKING socket:
     */
    opts = fcntl(SOSD.net.server_socket_fd, F_GETFL);
    if (opts < 0) { dlog(0, "[%s]: ERROR!  Cannot call fcntl() on the server_socket_fd to get its options.  Carrying on.  (%s)\n", whoami, strerror(errno)); }
 
    opts = opts & !(O_NONBLOCK);
    i    = fcntl(SOSD.net.server_socket_fd, F_SETFL, opts);
    if (i < 0) { dlog(0, "[%s]: ERROR!  Cannot use fcntl() to set the server_socket_fd to BLOCKING more.  Carrying on.  (%s).\n", whoami, strerror(errno)); }


    listen( SOSD.net.server_socket_fd, SOSD.net.listen_backlog );
    dlog(0, "[%s]: Listening on socket.\n", whoami);

    return;
}
 


void SOSD_init_database() {

    SOSD_db_init_database();
    SOSD_db_create_tables();
    SOSD.db_ready = 1;

    return;
}



void SOSD_init() {
    SOS_SET_WHOAMI(whoami, "SOSD_init");
    pid_t pid, ppid, sid;
    int rc;

    /* [daemon name]
     *     assign a name appropriate for whether it is participating in a cloud or not
     */
    snprintf(SOSD.daemon_name, SOS_DEFAULT_STRING_LEN, "%s", SOSD_DAEMON_NAME);


    /* [lock file]
     *     create and hold lock file to prevent multiple daemon spawn
     */
    #ifdef SOSD_CLOUD_SYNC
    snprintf(SOSD.lock_file, SOS_DEFAULT_STRING_LEN, "%s.%d.lock", SOSD.daemon_name, SOS.config.comm_rank);
    #else
    snprintf(SOSD.lock_file, SOS_DEFAULT_STRING_LEN, "%s.lock", SOSD.daemon_name);
    #endif
    sos_daemon_lock_fptr = open(SOSD.lock_file, O_RDWR | O_CREAT, 0640);
    if (sos_daemon_lock_fptr < 0) { 
        fprintf(stderr, "\n[%s]: ERROR!  Unable to start daemon (%s): Could not access lock file %s in directory %s\n", whoami, SOSD.daemon_name, SOSD.lock_file, SOSD.work_dir);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    if (lockf(sos_daemon_lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\n[%s]: ERROR!  Unable to start daemon (%s): AN INSTANCE IS ALREADY RUNNING!\n", whoami, SOSD.daemon_name);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[%s]: Lock file obtained.  (%s)\n", whoami, SOSD.lock_file); fflush(stdout); }


    /* [log file]
     *      system logging initialize
     */
    #ifdef SOSD_CLOUD_SYNC
    snprintf(SOSD.log_file, SOS_DEFAULT_STRING_LEN, "%s.%d.log", SOSD.daemon_name, SOS.config.comm_rank);
    #else
    snprintf(SOSD.log_file, SOS_DEFAULT_STRING_LEN, "%s.log", SOSD.daemon_name);
    #endif
    if (SOSD_DAEMON_LOG > 0) {
        if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[%s]: Opening log file: %s\n", whoami, SOSD.log_file); fflush(stdout); }
        sos_daemon_log_fptr = fopen(SOSD.log_file, "w");
        if ((SOS_DEBUG > 0) && SOSD_ECHO_TO_STDOUT) { printf("[%s]:   ... done.\n", SOSD.daemon_name); fflush(stdout); }
    }


    if (!SOSD_ECHO_TO_STDOUT) {
        dlog(1, "[%s]: Logging output up to this point has been suppressed, but all initialization has gone well.\n", whoami);
        dlog(1, "[%s]: Log file is now open.  Proceeding...\n", whoami);
        dlog(1, "[%s]: SOSD_init():\n", whoami);
    }

    /* [mode]
     *      interactive or detached/daemon
     */
    #if (SOSD_DAEMON_MODE > 0)
    {
    dlog(1, "[%s]:   ...mode: DETACHED DAEMON (fork/umask/sedsid)\n", whoami);
        /* [fork]
         *     split off from the parent process (& terminate parent)
         */
        ppid = getpid();
        pid  = fork();
        
        if (pid < 0) {
            dlog(0, "[%s]: ERROR! Unable to start daemon (%s): Could not fork() off parent process.\n", whoami, SOSD.daemon_name);
            exit(EXIT_FAILURE);
        }
        if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent
        
        /* [child session]
         *     create/occupy independent session from parent process
         */
        umask(0);
        sid = setsid();
        if (sid < 0) {
            dlog(0, "[%s]: ERROR!  Unable to start daemon (%s): Could not acquire a session id.\n", whoami, SOSD_DAEMON_NAME); 
            exit(EXIT_FAILURE);
        }
        if ((chdir(SOSD.work_dir)) < 0) {
            dlog(0, "[%s]: ERROR!  Unable to start daemon (%s): Could not change to working directory: %s\n", whoami, SOSD_DAEMON_NAME, SOSD.work_dir);
            exit(EXIT_FAILURE);
        }
        
        dlog(1, "[%s]:   ... session(%d) successfully split off from parent(%d).\n", whoami, getpid(), ppid);
    }
    #else
    {
        dlog(1, "[%s]:   ... mode: ATTACHED INTERACTIVE\n", whoami);
    }
    #endif

    sprintf(SOSD.daemon_pid_str, "%d", getpid());
    dlog(1, "[%s]:   ... pid: %s\n", whoami, SOSD.daemon_pid_str);

    /* Now we can write our PID out to the lock file safely... */
    rc = write(sos_daemon_lock_fptr, SOSD.daemon_pid_str, strlen(SOSD.daemon_pid_str));


    /* [file handles]
     *     close unused IO handles
     */

    if (SOS_DEBUG == 0) {
        dlog(1, "[%s]: Closing traditional I/O for the daemon...\n", whoami);
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    /* [guid's]
     *     configure the issuer of guids for this daemon
     */
    dlog(1, "[%s]: Obtaining this instance's guid range...\n", whoami);
    #ifdef SOSD_CLOUD_SYNC
        long guid_block_size = (long) ((long double) SOS_DEFAULT_UID_MAX / (long double) SOS.config.comm_size);
        long guid_my_first   = (long) SOS.config.comm_rank * guid_block_size;
        SOS_uid_init(&SOSD.guid, guid_my_first, (guid_my_first + (guid_block_size - 1)));
    #else
        SOS_uid_init(&SOSD.guid, 1, SOS_DEFAULT_UID_MAX);
    #endif
    dlog(1, "[%s]:   ... (%ld ---> %ld)\n", whoami, SOSD.guid->next, SOSD.guid->last);

    /* [hashtable]
     *    storage system for received pubs.  (will enque their key -> db)
     */
    dlog(1, "[%s]: Setting up a hash table for pubs...\n", whoami);
    SOSD.pub_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(1, "[%s]: Daemon initialization is complete.\n", whoami);
    SOSD.daemon_running = 1;
    return;
}

void SOSD_claim_guid_block(SOS_uid *id, int size, long *pool_from, long *pool_to) {
    SOS_SET_WHOAMI(whoami, "SOSD_guid_claim_range");

    #if (SOS_CONFIG_USE_MUTEXES > 0)
    pthread_mutex_lock( id->lock );
    #endif


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



    #if (SOS_CONFIG_USE_MUTEXES > 0)
    pthread_mutex_unlock( id->lock );
    #endif

    return;
}

void SOSD_apply_announce( SOS_pub *pub, char *msg, int msg_size ) {
    SOS_SET_WHOAMI(whoami, "SOSD_apply_announce");

    SOS_val_type   val_type;
    SOS_msg_header header;
    int            i;
    int            new_elem_count;
    long           new_guid;
    int            str_len;
    int            val_id;
    int            val_name_len;
    char          *val_name;
    int            ptr;
    int            first_announce;

    if (pub->elem_count < 1) {
        first_announce = 1;
        dlog(6, "[%s]: Applying a first announce to this publication, as the receiving end's pub handle is empty.\n", whoami);
    } else {
        first_announce = 0;
        dlog(6, "[%s]: Applying a re-announce.\n", whoami);
    }
    dlog(6, "[%s]:   ... pub->elem_count=%d   pub->elem_max=%d   msg_size=%d\n", whoami, pub->elem_count, pub->elem_max, msg_size);

    if (!first_announce) {
        /* Free all existing strings, they will be overwritten... */
        free(pub->node_id);
        free(pub->prog_name);
        free(pub->prog_ver);
        if (pub->pragma_len > 0) free(pub->pragma_msg);
        free(pub->title);
        for (i = 0; i < pub->elem_count; i++) {
            free( pub->data[i]->name );
        }
    }

    dlog(6, "[%s]:   ... extracting values from the message: ", whoami);
    ptr = 0;
    /* Extract the MESSAGE header ... */
    memcpy(&( header ), (msg + ptr), sizeof(SOS_msg_header));     ptr += sizeof(SOS_msg_header);
    dlog(6, "[msg_header(%d)] ", ptr);

    /* Process the PUB HEADER ... */
    memcpy(&( str_len            ), (msg + ptr), sizeof(int));    ptr += sizeof(int);     //dlog(6, "{ pub_id(%d), ", str_len);
    pub->node_id =       strndup(   (msg + ptr), str_len );       ptr += str_len;
    memcpy(&( pub->process_id    ), (msg + ptr), sizeof(int));    ptr += sizeof(int);
    memcpy(&( pub->thread_id     ), (msg + ptr), sizeof(int));    ptr += sizeof(int);
    memcpy(&( pub->comm_rank     ), (msg + ptr), sizeof(int));    ptr += sizeof(int);
    memcpy(&( str_len            ), (msg + ptr), sizeof(int));    ptr += sizeof(int);     //dlog(6, " prog_name(%d), ", str_len);
    pub->prog_name =     strndup(   (msg + ptr), str_len );       ptr += str_len;
    memcpy(&( str_len            ), (msg + ptr), sizeof(int));    ptr += sizeof(int);     //dlog(6, " prog_ver(%d), ", str_len);
    pub->prog_ver =      strndup(   (msg + ptr), str_len );       ptr += str_len;
    memcpy(&( pub->pragma_len    ), (msg + ptr), sizeof(int));    ptr += sizeof(int);     //dlog(6, " pragma_len(%d), ", pub->pragma_len);
    pub->pragma_msg = (char *) malloc(sizeof(char) * ( 1 + pub->pragma_len));
    memset(&( pub->pragma_msg    ), '\0', (1 + pub->pragma_len));
    memcpy(&( pub->pragma_msg    ), (msg + ptr), pub->pragma_len );   ptr += pub->pragma_len;
    memcpy(&( str_len            ), (msg + ptr), sizeof(int));    ptr += sizeof(int);     //dlog(6, " title(%d) }\n", str_len);
    pub->title =         strndup(   (msg + ptr), str_len );       ptr += str_len;
    memcpy(&( new_elem_count     ), (msg + ptr), sizeof(int));    ptr += sizeof(int);

    while ( pub->elem_max < new_elem_count ) SOS_expand_data( pub );
    pub->elem_count = new_elem_count;
    dlog(6, "[pub_head(%d)] ", ptr);

    /* Process the PUB METADATA ... */
    memcpy(&( pub->meta.channel     ), (msg + ptr), sizeof(int)); ptr += sizeof(int);
    memcpy(&( pub->meta.layer       ), (msg + ptr), sizeof(int)); ptr += sizeof(int);
    memcpy(&( pub->meta.nature      ), (msg + ptr), sizeof(int)); ptr += sizeof(int);
    memcpy(&( pub->meta.pri_hint    ), (msg + ptr), sizeof(int)); ptr += sizeof(int);
    memcpy(&( pub->meta.scope_hint  ), (msg + ptr), sizeof(int)); ptr += sizeof(int);
    memcpy(&( pub->meta.retain_hint ), (msg + ptr), sizeof(int)); ptr += sizeof(int);
    dlog(6, "[pub_meta(%d)] ", ptr);

    /* Process the PUB DATA ELEMENTS ... */
    for (i = 0; i < new_elem_count; i++) {
        if (ptr >= msg_size) break;

        memcpy(&( pub->data[i]->guid              ), (msg + ptr), sizeof(long));  ptr += sizeof(long);
        pub->data[i]->guid = new_guid;
        if (!first_announce) free( pub->data[i]->name );
        memcpy(&( str_len               ), (msg + ptr), sizeof(int));  ptr += sizeof(int);
        pub->data[i]->name =    strndup(   (msg + ptr), str_len);      ptr += str_len;
        memcpy(&( pub->data[i]->type    ), (msg + ptr), sizeof(int));  ptr += sizeof(int);

        pub->data[i]->state = SOS_VAL_STATE_EMPTY;  /* All Announcements set data[] to empty, until a pub comes in. */
    }
    dlog(6, "[pub_data(%d)]\n", ptr);
    dlog(6, "[%s]:   ... new_elem_count = %d.\n", whoami, new_elem_count);

    i = new_elem_count;
    while ( i < pub->elem_max ) { pub->data[i]->state = SOS_VAL_STATE_EMPTY; i++; }

    dlog(6, "[%s]:   ... done.\n", whoami);

    return;

}



void SOSD_apply_publish( SOS_pub *pub, char *msg, int msg_len ) {
    SOS_SET_WHOAMI(whoami, "SOSD_apply_publish");

    SOS_msg_header header;
    double recv_ts;
    int    dirty_elem_count;
    int    ptr;
    int    i;
    /* For loading data values: */
    int    index;
    double pack_ts;
    double send_ts;
    int    val_len;
    
    SOS_TIME( recv_ts );
    dlog(6, "[%s]: Applying updated values to pub{%s} ...\n", whoami, pub->title);

    ptr = 0;
    memcpy(&header,    (msg + ptr), sizeof(SOS_msg_header)); ptr += sizeof(SOS_msg_header);
    memcpy(&dirty_elem_count, (msg + ptr), sizeof(int));     ptr += sizeof(int);

    dlog(6, "[%s]:   ... pub->guid = %ld     dirty_elem_count = %d\n", whoami, header.pub_guid, dirty_elem_count);

    for (i = 0; i < dirty_elem_count; i++) {
        memcpy(&index,        (msg + ptr), sizeof(int));     ptr += sizeof(int);
        memcpy(&pack_ts,      (msg + ptr), sizeof(double));  ptr += sizeof(double);
        memcpy(&send_ts,      (msg + ptr), sizeof(double));  ptr += sizeof(double);
        memcpy(&val_len,      (msg + ptr), sizeof(int));     ptr += sizeof(int);

        if (index >= pub->elem_count) {
            dlog(1, "[%s]:   ...\n", whoami);
            dlog(1, "[%s]:   ... ERROR!  Attempting to apply published data into an index larger than\n", whoami);
            dlog(1, "[%s]:   ...         the announced publication handle's highest element index.\n", whoami);
            dlog(1, "[%s]:   ...\n", whoami);
            dlog(1, "[%s]:   ...    pub{%s}->elem_count == %d;  (incoming) index == %d;\n", whoami, pub->title, pub->elem_count, index);
            dlog(1, "[%s]:   ...\n", whoami);
            dlog(1, "[%s]:   ...  (Discarding this element update and skipping ahead.)\n", whoami);
            dlog(1, "[%s]:   ... ----------\n", whoami);
            ptr += val_len;
            continue;
        }

        dlog(6, "[%s]:   ... Value received for: data[%d]->name(\"%s\") = ", whoami, index, pub->data[index]->name);

        pub->data[index]->time.pack = pack_ts;
        pub->data[index]->time.send = send_ts;
        pub->data[index]->time.recv = recv_ts;
        pub->data[index]->state     = SOS_VAL_STATE_DIRTY;

        switch (pub->data[index]->type) {
        case SOS_VAL_TYPE_INT :    memcpy(&(pub->data[index]->val.i_val), (msg + ptr), val_len); ptr += val_len; dlog(6, "%d\n",  pub->data[index]->val.i_val); break;
        case SOS_VAL_TYPE_LONG :   memcpy(&(pub->data[index]->val.l_val), (msg + ptr), val_len); ptr += val_len; dlog(6, "%ld\n", pub->data[index]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE : memcpy(&(pub->data[index]->val.d_val), (msg + ptr), val_len); ptr += val_len; dlog(6, "%lf\n", pub->data[index]->val.d_val); break;
        case SOS_VAL_TYPE_STRING :
            if (pub->data[index]->val.c_val != NULL) { 
                free(pub->data[index]->val.c_val);
            }
            pub->data[index]->val.c_val = (char *) malloc((1 + val_len) * sizeof(char));
            memset(pub->data[index]->val.c_val, '\0', (1 + val_len));
            memcpy(pub->data[index]->val.c_val, (msg + ptr), val_len); ptr += (val_len);
            dlog(6, "\"%s\"   (val_len == %d)\n", pub->data[index]->val.c_val, val_len); break;
        }
    }

    dlog(6, "[%s]:   ... Done.\n", whoami);

    return;
}





