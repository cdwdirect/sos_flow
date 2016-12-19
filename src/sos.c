
/*
 * sos.c                 SOS library routines
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netdb.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_debug.h"
#include "sos_buffer.h"
#include "sos_pipe.h"
#include "sos_qhashtbl.h"

/* Private functions (not in the header file) */
void*        SOS_THREAD_feedback(void *arg);
void         SOS_handle_feedback(SOS_buffer *buffer);
void         SOS_expand_data(SOS_pub *pub);

SOS_runtime* SOS_init_with_runtime(int *argc, char ***argv, SOS_role role, SOS_layer layer, SOS_runtime *extant_sos_runtime);



/* **************************************** */
/* [util]                                   */
/* **************************************** */

SOS_runtime* SOS_init( int *argc, char ***argv, SOS_role role, SOS_layer layer) {
    return SOS_init_with_runtime(argc, argv, role, layer, NULL);
}

SOS_runtime* SOS_init_with_runtime( int *argc, char ***argv, SOS_role role, SOS_layer layer, SOS_runtime *extant_sos_runtime ) {
    SOS_msg_header header;
    unsigned char buffer[SOS_DEFAULT_REPLY_LEN] = {0};
    int i, n, retval, server_socket_fd;
    SOS_guid guid_pool_from;
    SOS_guid guid_pool_to;


    SOS_runtime *NEW_SOS = NULL;
    if (extant_sos_runtime == NULL) {
        NEW_SOS = (SOS_runtime *) malloc(sizeof(SOS_runtime));
        memset(NEW_SOS, '\0', sizeof(SOS_runtime));
    } else {
        NEW_SOS = extant_sos_runtime;
    }

    /*
     *  Before SOS_init returned a unique context per caller, we wanted
     *  to make it re-entrant.  This saved mistakes from being made when
     *  multiple parts of a single binary were independently instrumented
     *  with SOS calls reflecting different layers or metadata.
     *
     *  The way it works now, wrappers and libraries and applications
     *  can all have their own unique contexts and metadata, this is better.
     *
    static bool _initialized = false;   //old way
    if (_initialized) return;
    _initialized = true;
    */

    
    NEW_SOS->config.offline_test_mode = false;
    NEW_SOS->config.runtime_utility   = false;

    switch (role) {
    case SOS_ROLE_OFFLINE_TEST_MODE:
        NEW_SOS->config.offline_test_mode = true;
        NEW_SOS->role = SOS_ROLE_CLIENT;
        break;
    case SOS_ROLE_RUNTIME_UTILITY:
        NEW_SOS->config.runtime_utility   = true;
        NEW_SOS->role = SOS_ROLE_CLIENT;
        break;
    default:
        NEW_SOS->role = role;
        break;
    }

    NEW_SOS->status = SOS_STATUS_INIT;
    NEW_SOS->config.layer  = layer;
    SOS_SET_CONTEXT(NEW_SOS, "SOS_init");

    dlog(1, "Initializing SOS ...\n");
    dlog(1, "  ... setting argc / argv\n");

    if (argc == NULL || argv == NULL) {
        dlog(1, "NOTE: argc == NULL || argv == NULL, using safe but meaningles values.\n");
        SOS->config.argc = 2;
        SOS->config.argv = (char **) malloc(2 * sizeof(char *));
        SOS->config.argv[0] = strdup("[NULL]");
        SOS->config.argv[1] = strdup("[NULL]");
    } else {
        SOS->config.argc = *argc;
        SOS->config.argv = *argv;
    }

    SOS->config.process_id = (int) getpid();

    SOS->config.node_id = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    gethostname( SOS->config.node_id, SOS_DEFAULT_STRING_LEN );
    dlog(1, "  ... node_id: %s\n", SOS->config.node_id );

    if (SOS->role == SOS_ROLE_CLIENT) {
        SOS->config.locale = SOS_LOCALE_APPLICATION;

        if (SOS->config.runtime_utility == false) {
            dlog(1, "  ... launching libsos runtime thread[s].\n");
            SOS->task.feedback =            (pthread_t *) malloc(sizeof(pthread_t));
            SOS->task.feedback_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
            SOS->task.feedback_cond =  (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
            retval = pthread_create( SOS->task.feedback, NULL, SOS_THREAD_feedback, (void *) SOS );
            if (retval != 0) { dlog(0, " ... ERROR (%d) launching SOS->task.feedback thread!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
            retval = pthread_mutex_init(SOS->task.feedback_lock, NULL);
            if (retval != 0) { dlog(0, " ... ERROR (%d) creating SOS->task.feedback_lock!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
            retval = pthread_cond_init(SOS->task.feedback_cond, NULL);
            if (retval != 0) { dlog(0, " ... ERROR (%d) creating SOS->task.feedback_cond!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
        }
        SOS->net.send_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        retval = pthread_mutex_init(SOS->net.send_lock, NULL);
        if (retval != 0) { dlog(0, " ... ERROR (%d) creating SOS->net.send_lock!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
    }

    if (SOS->config.offline_test_mode == true) {
        /* Here, the offline mode finishes up any non-networking initialization and bails out. */
        SOS_uid_init(SOS, &SOS->uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
        SOS_uid_init(SOS, &SOS->uid.my_guid_pool, 0, SOS_DEFAULT_UID_MAX);
        SOS->my_guid = SOS_uid_next( SOS->uid.my_guid_pool );
        SOS->status = SOS_STATUS_RUNNING;
        dlog(1, "  ... done with SOS_init().  [OFFLINE_TEST_MODE]\n");
        dlog(1, "SOS->status = SOS_STATUS_RUNNING\n");
        return SOS;
    }

    if (SOS->role == SOS_ROLE_CLIENT) {
        /* NOTE: This is only used for clients.  Daemons handle their own. */
        char *env_rank;
        char *env_size;

        env_rank = getenv("PMI_RANK");
        env_size = getenv("PMI_SIZE");
        if ((env_rank!= NULL) && (env_size != NULL)) {
            /* MPICH_ */
            SOS->config.comm_rank = atoi(env_rank);
            SOS->config.comm_size = atoi(env_size);
            dlog(1, "  ... MPICH environment detected. (rank: %d/ size:%d)\n", SOS->config.comm_rank, SOS->config.comm_size);
        } else {
            env_rank = getenv("OMPI_COMM_WORLD_RANK");
            env_size = getenv("OMPI_COMM_WORLD_SIZE");
            if ((env_rank != NULL) && (env_size != NULL)) {
                /* OpenMPI */
                SOS->config.comm_rank = atoi(env_rank);
                SOS->config.comm_size = atoi(env_size);
                dlog(1, "  ... OpenMPI environment detected. (rank: %d/ size:%d)\n", SOS->config.comm_rank, SOS->config.comm_size);
            } else {
                /* non-MPI client. */
                SOS->config.comm_rank = 0;
                SOS->config.comm_size = 1;
                dlog(1, "  ... Non-MPI environment detected. (rank: %d/ size:%d)\n", SOS->config.comm_rank, SOS->config.comm_size);
            }
        }
    }

    if (SOS->role == SOS_ROLE_CLIENT) {
        /*
         *
         *  CLIENT
         *
         */
        dlog(1, "  ... setting up socket communications with the daemon.\n" );

        SOS_buffer_init(SOS, &SOS->net.recv_part);

        SOS->net.buffer_len    = SOS_DEFAULT_BUFFER_MAX;
        SOS->net.timeout       = SOS_DEFAULT_MSG_TIMEOUT;
        SOS->net.server_host   = SOS_DEFAULT_SERVER_HOST;
        SOS->net.server_port   = getenv("SOS_CMD_PORT");
        if ( SOS->net.server_port == NULL ) { fprintf(stderr, "ERROR!  SOS_CMD_PORT environment variable is not set!\n"); exit(EXIT_FAILURE); }
        if ( strlen(SOS->net.server_port) < 2 ) { fprintf(stderr, "ERROR!  SOS_CMD_PORT environment variable is not set!\n"); exit(EXIT_FAILURE); }

        SOS->net.server_hint.ai_family    = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
        SOS->net.server_hint.ai_protocol  = 0;                /* Any protocol */
        SOS->net.server_hint.ai_socktype  = SOCK_STREAM;      /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
        SOS->net.server_hint.ai_flags     = AI_NUMERICSERV | SOS->net.server_hint.ai_flags;

            retval = getaddrinfo(SOS->net.server_host, SOS->net.server_port, &SOS->net.server_hint, &SOS->net.result_list );
            if ( retval < 0 ) { dlog(0, "ERROR!  Could not locate the SOS daemon.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port ); exit(1); }

        for ( SOS->net.server_addr = SOS->net.result_list ; SOS->net.server_addr != NULL ; SOS->net.server_addr = SOS->net.server_addr->ai_next ) {
            /* Iterate the possible connections and register with the SOS daemon: */
            server_socket_fd = socket(SOS->net.server_addr->ai_family, SOS->net.server_addr->ai_socktype, SOS->net.server_addr->ai_protocol );
            if ( server_socket_fd == -1 ) continue;
            if ( connect(server_socket_fd, SOS->net.server_addr->ai_addr, SOS->net.server_addr->ai_addrlen) != -1 ) break; /* success! */
            close( server_socket_fd );
        }

        freeaddrinfo( SOS->net.result_list );
        
        if (server_socket_fd == 0) { dlog(0, "ERROR!  Could not connect to the server.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port); exit(1); }

        dlog(1, "  ... registering this instance with SOS->   (%s:%s)\n", SOS->net.server_host, SOS->net.server_port);

        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = 0;
        header.pub_guid = 0;

        SOS_buffer *buffer;
        SOS_buffer_init_sized_locking(SOS, &buffer, 64, false);
        
        int offset = 0;
        SOS_buffer_pack(buffer, &offset, "iigg", 
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);

        header.msg_size = offset;
        offset = 0;
        SOS_buffer_pack(buffer, &offset, "i", header.msg_size);

        pthread_mutex_lock(SOS->net.send_lock);

        dlog(1, "Built a registration message:\n");
        dlog(1, "  ... buffer->data == %ld\n", (long) buffer->data);
        dlog(1, "  ... buffer->len  == %d\n", buffer->len);
        dlog(1, "Calling send...\n");

        retval = send( server_socket_fd, buffer->data, buffer->len, 0);

        if (retval < 0) {
            dlog(0, "ERROR!  Could not write to server socket!  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port);
            exit(EXIT_FAILURE);
        } else {
            dlog(1, "   ... registration message sent.   (retval == %d)\n", retval);
        }

        SOS_buffer_wipe(buffer);

        dlog(1, "  ... listening for the server to reply...\n");
        buffer->len = recv( server_socket_fd, (void *) buffer->data, buffer->max, 0);
        dlog(6, "  ... server responded with %d bytes.\n", retval);

        close( server_socket_fd );
        pthread_mutex_unlock(SOS->net.send_lock);

        offset = 0;
        SOS_buffer_unpack(buffer, &offset, "gg",
                          &guid_pool_from,
                          &guid_pool_to);
        dlog(1, "  ... received guid range from %" SOS_GUID_FMT " to %" SOS_GUID_FMT ".\n", guid_pool_from, guid_pool_to);
        dlog(1, "  ... configuring uid sets.\n");

        SOS_uid_init(SOS, &SOS->uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
        SOS_uid_init(SOS, &SOS->uid.my_guid_pool, guid_pool_from, guid_pool_to);   /* LISTENER doesn't use this, it's for CLIENTS. */

        SOS->my_guid = SOS_uid_next( SOS->uid.my_guid_pool );
        dlog(1, "  ... SOS->my_guid == %" SOS_GUID_FMT "\n", SOS->my_guid);

        SOS_buffer_destroy(buffer);


    } else {
        /*
         *
         *  CONFIGURATION: LISTENER / AGGREGATOR / etc.
         *
         */

        dlog(1, "  ... skipping socket setup (becase we're the daemon).\n");
        
    }

    SOS->status = SOS_STATUS_RUNNING;

    dlog(1, "  ... done with SOS_init().\n");
    dlog(1, "SOS->status = SOS_STATUS_RUNNING\n");
    return SOS;
}


/* TODO: { FEEDBACK } */
int SOS_sense_register(SOS_runtime *sos_context, char *handle, void (*your_callback)(void *data)) {
    SOS_SET_CONTEXT(sos_context, "SOS_sense_register");



    return 0;
}


/* TODO: { FEEDBACK } */
void SOS_sense_activate(SOS_runtime *sos_context, char *handle, SOS_layer layer, void *data, int data_length) {
    SOS_SET_CONTEXT(sos_context, "SOS_sense_activate");

    return;
}



void SOS_send_to_daemon(SOS_buffer *send_buffer, SOS_buffer *reply_buffer ) {
    SOS_SET_CONTEXT(send_buffer->sos_context, "SOS_send_to_daemon");
    SOS_msg_header header;
    int            server_socket_fd;
    int            offset      = 0;
    int            inset       = 0;
    int            retval      = 0;
    double         time_start  = 0.0;
    double         time_out    = 0.0;

    if (SOS->status == SOS_STATUS_SHUTDOWN) {
        dlog(1, "Suppressing a send to the daemon.  (SOS_STATUS_SHUTDOWN)\n");
        return;
    }

    if (SOS->config.offline_test_mode == true) {
        dlog(1, "Suppressing a send to the daemon.  (OFFLINE_TEST_MODE)\n");
        return;
    }

    pthread_mutex_lock(SOS->net.send_lock);
    dlog(6, "Processing a send to the daemon.\n");

    retval = getaddrinfo(SOS->net.server_host, SOS->net.server_port, &SOS->net.server_hint, &SOS->net.result_list );
    if ( retval < 0 ) { dlog(0, "ERROR!  Could not locate the SOS daemon.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port ); exit(1); }
    
    /* Iterate the possible connections and register with the SOS daemon: */
    for ( SOS->net.server_addr = SOS->net.result_list ; SOS->net.server_addr != NULL ; SOS->net.server_addr = SOS->net.server_addr->ai_next ) {
        server_socket_fd = socket(SOS->net.server_addr->ai_family, SOS->net.server_addr->ai_socktype, SOS->net.server_addr->ai_protocol );
        if ( server_socket_fd == -1 ) continue;
        if ( connect( server_socket_fd, SOS->net.server_addr->ai_addr, SOS->net.server_addr->ai_addrlen) != -1 ) break; /* success! */
        close( server_socket_fd );
    }
    
    freeaddrinfo( SOS->net.result_list );
    
    if (server_socket_fd == 0) {
        dlog(0, "Error attempting to connect to the server.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port);
        exit(1);
    }

    int more_to_send      = 1;
    int failed_send_count = 0;
    int total_bytes_sent  = 0;
    retval = 0;

    SOS_TIME(time_start);
    while (more_to_send) {
        if (failed_send_count > 8) {
            dlog(0, "ERROR: Unable to contact sosd daemon after 8 attempts. Terminating.\n");
            exit(EXIT_FAILURE);
        }
        retval = send(server_socket_fd, (send_buffer->data + retval), send_buffer->len, 0 );
        if (retval < 0) {
            failed_send_count++;
            dlog(0, "ERROR: Could not send message to daemon. (%s)\n", strerror(errno));
            dlog(0, "ERROR:    ...retrying %d more times after a brief delay.\n", (8 - failed_send_count));
            usleep(100000);
            continue;
        } else {
            total_bytes_sent += retval;
        }
        if (total_bytes_sent >= send_buffer->len) {
            more_to_send = 0;
        }
    }//while

    dlog(6, "Send complete, waiting for a reply...\n");

    offset = 0;

    /* TODO: { COMMUNICATION, SOCKET } Make this a nice loop like the daemon... */
    reply_buffer->len = recv(server_socket_fd, reply_buffer->data, reply_buffer->max, 0);

    close( server_socket_fd );
    pthread_mutex_unlock(SOS->net.send_lock);

    dlog(6, "Reply fully received.  reply_buffer->len == %d\n", reply_buffer->len);

    return;
}



void SOS_finalize(SOS_runtime *sos_context) {
    SOS_SET_CONTEXT(sos_context, "SOS_finalize");
    
    /* This will cause any SOS threads to leave their loops next time they wake up. */
    dlog(1, "SOS->status = SOS_STATUS_SHUTDOWN\n");
    SOS->status = SOS_STATUS_SHUTDOWN;

    free(SOS->config.node_id);

    if (SOS->role == SOS_ROLE_CLIENT) {
        if (SOS->config.runtime_utility == false) {
            dlog(1, "  ... Joining threads...\n");
            pthread_cond_signal(SOS->task.feedback_cond);
            pthread_join(*SOS->task.feedback, NULL);
            pthread_cond_destroy(SOS->task.feedback_cond);
            pthread_mutex_lock(SOS->task.feedback_lock);
            pthread_mutex_destroy(SOS->task.feedback_lock);
            free(SOS->task.feedback_lock);
            free(SOS->task.feedback_cond);
            free(SOS->task.feedback);
        }

        dlog(1, "  ... Removing send lock...\n");
        pthread_mutex_lock(SOS->net.send_lock);
        pthread_mutex_destroy(SOS->net.send_lock);
        free(SOS->net.send_lock);
    
        dlog(1, "  ... Releasing uid objects...\n");
        SOS_uid_destroy(SOS->uid.local_serial);
        SOS_uid_destroy(SOS->uid.my_guid_pool);
        
        if (SOS->config.offline_test_mode == false) {
            dlog(1, "  ... Clearing up networking...\b");
            SOS_buffer_destroy(SOS->net.recv_part);
        }
    }    

    dlog(1, "Done!\n");
    free(SOS);

    return;
}



void* SOS_THREAD_feedback( void *args ) {
    SOS_runtime *local_ptr_to_context = (SOS_runtime *) args;
    SOS_SET_CONTEXT(local_ptr_to_context, "SOS_THREAD_feedback");
    struct timespec ts;
    struct timeval  tp;
    int wake_type;
    int error_count;

    int offset;
    SOS_msg_header  header;
    SOS_buffer     *check_in_buffer;
    SOS_buffer     *feedback_buffer;
    SOS_feedback    feedback;

    if ( SOS->config.offline_test_mode == true ) { return NULL; }

    SOS_buffer_init(SOS, &check_in_buffer);
    SOS_buffer_init(SOS, &feedback_buffer);

    while (SOS->status != SOS_STATUS_RUNNING && SOS->status != SOS_STATUS_SHUTDOWN) {
        usleep(1000);
    };

    /* Set the wakeup time (ts) to 2 seconds in the future. */
    gettimeofday(&tp, NULL);
    ts.tv_sec  = (tp.tv_sec + 2);
    ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;

    /* Grab the lock that the wakeup condition is bound to. */
    pthread_mutex_lock(SOS->task.feedback_lock);
    error_count = 0;

    while (SOS->status != SOS_STATUS_SHUTDOWN) {
        /* Build a checkin message. */
        SOS_buffer_wipe(check_in_buffer);
        SOS_buffer_wipe(feedback_buffer);

        header.msg_size = -1;
        header.msg_from = SOS->my_guid;
        header.msg_type = SOS_MSG_TYPE_CHECK_IN;
        header.pub_guid = 0;

        dlog(4, "Building a check-in message.\n");

        offset = 0;
        SOS_buffer_pack(check_in_buffer, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);

        header.msg_size = offset;
        offset = 0;
        SOS_buffer_pack(check_in_buffer, &offset, "i", header.msg_size);

        if (SOS->status != SOS_STATUS_RUNNING) break;

        dlog(4, "Sending check-in to daemon.\n");

        /* Ping the daemon to see if there is anything to do. */
        SOS_send_to_daemon(check_in_buffer, feedback_buffer);

        dlog(4, "Processing reply (to check-in)...\n");

        memset(&header, '\0', sizeof(SOS_msg_header));
        offset = 0;
        SOS_buffer_unpack(feedback_buffer, &offset, "iigg",
            &header.msg_size,
            &header.msg_type,
            &header.msg_from,
            &header.pub_guid);

        if (header.msg_type != SOS_MSG_TYPE_FEEDBACK) {
            dlog(0, "WARNING: sosd (daemon) responded to a CHECK_IN_MSG with malformed FEEDBACK!\n");
            error_count++;
            if (error_count > 5) { 
                dlog(0, "ERROR: Too much mal-formed feedback, shutting down feedback thread.\n");
                break;
            }
            gettimeofday(&tp, NULL);
            ts.tv_sec  = (tp.tv_sec + 2);
            ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;
            continue;
        } else {
            error_count = 0;
        }

        SOS_buffer_unpack(feedback_buffer, &offset, "i", &feedback);

        switch (feedback) {
        case SOS_FEEDBACK_CONTINUE: break;
        case SOS_FEEDBACK_EXEC_FUNCTION: 
        case SOS_FEEDBACK_SET_PARAMETER: 
        case SOS_FEEDBACK_EFFECT_CHANGE: 
            SOS_handle_feedback(feedback_buffer);
            break;

        default: break;
        }

        /* Set the timer to 2 seconds in the future. */
        gettimeofday(&tp, NULL);
        ts.tv_sec  = (tp.tv_sec + 2);
        ts.tv_nsec = (1000 * tp.tv_usec);
        /* Go to sleep until the wakeup time (ts) is reached. */
        wake_type = pthread_cond_timedwait(SOS->task.feedback_cond, SOS->task.feedback_lock, &ts);
        if (wake_type == ETIMEDOUT) {
            /* ...any special actions that need to happen if timed-out vs. explicitly triggered */
        }
    }

    SOS_buffer_destroy(check_in_buffer);
    SOS_buffer_destroy(feedback_buffer);
    pthread_mutex_unlock(SOS->task.feedback_lock);

    return NULL;
}


void SOS_handle_feedback(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_handle_feedback");
    int  activity_code;
    char function_sig[SOS_DEFAULT_STRING_LEN] = {0};

    SOS_msg_header header;

    int offset;

    dlog(4, "Determining appropriate action RE:feedback from daemon.\n");

    SOS_buffer_lock(buffer);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    SOS_buffer_unpack(buffer, &offset, "i",
        &activity_code);
    
    switch (activity_code) {
    case SOS_FEEDBACK_CONTINUE: break;
    case SOS_FEEDBACK_EXEC_FUNCTION:
        /* TODO: { FEEDBACK } */
        SOS_buffer_unpack(buffer, &offset, "s", function_sig);
        dlog(5, "FEEDBACK activity_code {%d} called --> EXEC_FUNCTION(%s) triggered.\n", activity_code, function_sig);
        break;

    case SOS_FEEDBACK_SET_PARAMETER: break;
    case SOS_FEEDBACK_EFFECT_CHANGE: break;
    default: break;
    }

    SOS_buffer_unlock(buffer);

    return;
}





void SOS_uid_init(SOS_runtime *sos_context,  SOS_uid **id_var, SOS_guid set_from, SOS_guid set_to ) {
    SOS_SET_CONTEXT(sos_context, "SOS_uid_init");
    SOS_uid *id;

    dlog(5, "  ... allocating uid sets\n");
    id = *id_var = (SOS_uid *) malloc(sizeof(SOS_uid));
    id->next = (set_from > 0) ? set_from : 1;
    id->last = (set_to   < SOS_DEFAULT_UID_MAX) ? set_to : SOS_DEFAULT_UID_MAX;
    dlog(5, "     ... default set for uid range (%" SOS_GUID_FMT " -> %" SOS_GUID_FMT ").\n", id->next, id->last);
    dlog(5, "     ... initializing uid mutex.\n");
    id->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(id->lock, NULL );

    id->sos_context = sos_context;

    return;
}

void SOS_uid_destroy( SOS_uid *id ) {
    SOS_SET_CONTEXT(id->sos_context, "SOS_uid_destroy");

    dlog(5, "  ... destroying uid mutex     &(%ld)\n", (long) &id->lock );
    pthread_mutex_lock( id->lock );
    pthread_mutex_destroy( id->lock );
    dlog(5, "  ... freeing uid mutex space  &(%ld)\n", (long) &id->lock );
    free(id->lock);
    dlog(5, "  ... freeing uid memory       &(%ld)\n", (long) id);
    memset(id, '\0', sizeof(SOS_uid));
    free(id);

    return;
}


SOS_guid SOS_uid_next( SOS_uid *id ) {
    SOS_SET_CONTEXT(id->sos_context, "SOS_uid_next");
    long next_serial;

    dlog(7, "LOCK id->lock\n");
    pthread_mutex_lock( id->lock );

    next_serial = id->next++;

    if (id->next > id->last) {
    /* The assumption here is that we're dealing with a GUID, as the other
     * 'local' uid ranges are so large as to effectively guarantee this case
     * will not occur for them.
     */

        if (SOS->role != SOS_ROLE_CLIENT) {
            /* NOTE: There is no recourse if a sosd daemon runs out of GUIDs.
             *       That should *never* happen.
             */
            dlog(0, "ERROR:  This sosd instance has run out of GUIDs!  Terminating.\n");
            exit(EXIT_FAILURE);
        } else {
            /* Acquire a fresh block of GUIDs from the sosd daemon... */
            SOS_msg_header header;
            SOS_buffer *buf;
            int offset;

            SOS_buffer_init_sized_locking(SOS, &buf, sizeof(SOS_msg_header), false);
            
            dlog(1, "The last guid has been used from SOS->uid.my_guid_pool!  Requesting a new block...\n");
            header.msg_size = -1;
            header.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
            header.msg_from = SOS->my_guid;
            header.pub_guid = 0;

            offset = 0;
            SOS_buffer_pack(buf, &offset, "iigg",
                            header.msg_size,
                            header.msg_type,
                            header.msg_from,
                            header.pub_guid);

            header.msg_size = offset;
            offset = 0;
            SOS_buffer_pack(buf, &offset, "i", header.msg_size);

            SOS_buffer *reply;
            SOS_buffer_init_sized_locking(SOS, &reply, (2 * sizeof(uint64_t)), false);

            SOS_send_to_daemon(buf, reply);

            if (SOS->config.offline_test_mode == true) {
                /* NOTE: In OFFLINE_TEST_MODE there is zero chance of exhausting GUID's... seriously. */
            } else {
                offset = 0;
                SOS_buffer_unpack(reply, &offset, "g", &id->next);
                SOS_buffer_unpack(reply, &offset, "g", &id->last);
                dlog(1, "  ... recieved a new guid block from %" SOS_GUID_FMT " to %" SOS_GUID_FMT ".\n", id->next, id->last);
            }
            SOS_buffer_destroy(buf);
            SOS_buffer_destroy(reply);
        }
    }

    dlog(7, "UNLOCK id->lock\n");
    pthread_mutex_unlock( id->lock );

    return next_serial;
}


SOS_pub* SOS_pub_create(SOS_runtime *sos_context, char *title, SOS_nature nature) {
    return SOS_pub_create_sized(sos_context, title, nature, SOS_DEFAULT_ELEM_MAX);
}

SOS_pub* SOS_pub_create_sized(SOS_runtime *sos_context, char *title, SOS_nature nature, int new_size) {
    SOS_SET_CONTEXT(sos_context, "SOS_pub_create_sized");
    SOS_pub   *new_pub;
    int        i;

    dlog(6, "Allocating and initializing a new pub handle....\n");

    new_pub = malloc(sizeof(SOS_pub));
    memset(new_pub, '\0', sizeof(SOS_pub));

    dlog(6, "  ... binding pub to it's execution context.\n");
    new_pub->sos_context = sos_context;

    dlog(6, "  ... configuring pub->lock\n");
    new_pub->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(new_pub->lock, NULL);
    pthread_mutex_lock(new_pub->lock);

    new_pub->sync_pending = 0; // Used by daemon/db to coordinate db injection.

    if (SOS->role != SOS_ROLE_CLIENT) {
        new_pub->guid = -1;
    } else {
        new_pub->guid = SOS_uid_next( SOS->uid.my_guid_pool );
    }
    snprintf(new_pub->guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, new_pub->guid);

    dlog(6, "  ... setting default values, allocating space for strings.\n");

    new_pub->process_id   = 0;
    new_pub->thread_id    = 0;
    new_pub->comm_rank    = SOS->config.comm_rank;
    new_pub->pragma_len   = 0;
    strcpy(new_pub->title, title);
    new_pub->announced           = 0;
    new_pub->elem_count          = 0;
    new_pub->elem_max            = new_size;
    new_pub->meta.channel     = 0;
    new_pub->meta.layer       = SOS->config.layer;
    new_pub->meta.nature      = nature;
    new_pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    new_pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    new_pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

    dlog(6, "  ... zero-ing out the strings.\n");

    /* Set some defaults for the SOS_ROLE_CLIENT's */
    if (SOS->role == SOS_ROLE_CLIENT) {
        dlog(6, "  ... setting defaults specific to SOS_ROLE_CLIENT.\n");
        strncpy(new_pub->node_id, SOS->config.node_id, SOS_DEFAULT_STRING_LEN);
        new_pub->process_id = SOS->config.process_id;
        strncpy(new_pub->prog_name, SOS->config.argv[0], SOS_DEFAULT_STRING_LEN);
    }

    dlog(6, "  ... allocating space for data elements.\n");
    new_pub->data                = malloc(sizeof(SOS_data *) * new_size);

    dlog(6, "  ... setting defaults for each data element.\n");
    for (i = 0; i < new_size; i++) {
        new_pub->data[i] = malloc(sizeof(SOS_data));
            memset(new_pub->data[i], '\0', sizeof(SOS_data));
            new_pub->data[i]->guid      = 0;
            new_pub->data[i]->name[0]   = '\0';
            new_pub->data[i]->type      = SOS_VAL_TYPE_INT;
            new_pub->data[i]->val_len   = 0;
            new_pub->data[i]->val.l_val = 0;
            new_pub->data[i]->val.c_val = 0;
            new_pub->data[i]->val.d_val = 0.0;
            new_pub->data[i]->state     = SOS_VAL_STATE_EMPTY;
            new_pub->data[i]->time.pack = 0.0;
            new_pub->data[i]->time.send = 0.0;
            new_pub->data[i]->time.recv = 0.0;

            new_pub->data[i]->meta.freq       = SOS_VAL_FREQ_DEFAULT;
            new_pub->data[i]->meta.classifier = SOS_VAL_CLASS_DATA;
            new_pub->data[i]->meta.semantic   = SOS_VAL_SEMANTIC_DEFAULT;
            new_pub->data[i]->meta.pattern    = SOS_VAL_PATTERN_DEFAULT;
            new_pub->data[i]->meta.compare    = SOS_VAL_COMPARE_SELF;
            new_pub->data[i]->meta.mood       = SOS_MOOD_GOOD;

    }

    if (SOS->role == SOS_ROLE_CLIENT) {
        dlog(6, "  ... configuring pub to use sos_context->task.val_intake for snap queue.\n");
        SOS_pipe_init((void *) SOS, &new_pub->snap_queue, sizeof(SOS_val_snap *));
    }

    dlog(6, "  ... initializing the name table for values.\n");
    new_pub->name_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(6, "  ... done.\n");
    pthread_mutex_unlock(new_pub->lock);

    return new_pub;
}




void SOS_expand_data( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_expand_data");

    /* NOTE: This is an internal-use-only function, and assumes you
     *       already hold the lock on the pub.
     */

    dlog(6, "Growing pub(\"%s\")->elem_max from %d to %d...\n",
            pub->title,
            pub->elem_max,
            (pub->elem_max + SOS_DEFAULT_ELEM_MAX));

    int n = 0;
    int from_old_max = pub->elem_max;
    int to_new_max   = pub->elem_max + SOS_DEFAULT_ELEM_MAX;

    pub->data = (SOS_data **) realloc(pub->data, (to_new_max * sizeof(SOS_data *)));

    for (n = from_old_max; n < to_new_max; n++) {
        pub->data[n] = calloc(1, sizeof(SOS_data));
    }

    pub->elem_max = to_new_max;

    dlog(6, "  ... done.\n");

    return;
}


void SOS_strip_str( char *str ) {
    int i, len;
    len = strlen(str);

    for (i = 0; i < len; i++) {
        if (str[i] == '\"') str[i] = '\'';
        if (str[i] < ' ' || str[i] > '~') str[i] = '#';
    }
  
    return;
}


char* SOS_uint64_to_str(uint64_t val, char *result, int result_len) {
    if (result_len < 128) {
        snprintf(result, result_len, "ERROR: result buffer < 128");
        return result;
    } else {
        snprintf(result, result_len, "%" SOS_GUID_FMT, val);
    }

    size_t i = 0;
    size_t i2 = i + (i / 3);
    int c = 0;
    result[i2 + 1] = 0;

    for(i = (strlen(result) - 1) ; i != 0; i-- ) {
        result[i2--] = result[i];
        c++;
        if( c % 3 == 0 ) {
            result[i2--] = ',';
        }
    }

    return result;  
}


int SOS_event(SOS_pub *pub, const char *name, SOS_val_semantic semantic) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_event");
    int pos = 0;
    int val = 1;

    pos = SOS_pub_search(pub, name);
    if (pos >= 0) {
        val = pub->data[pos]->val.i_val;
        val++;
    } else {
        val = 1;
    }

    pos = SOS_pack(pub, name, SOS_VAL_TYPE_INT, &val);

    pub->data[pos]->meta.classifier = SOS_VAL_CLASS_EVENT;
    pub->data[pos]->meta.semantic   = semantic;

    return pos;
}


int SOS_pack( SOS_pub *pub, const char *name, SOS_val_type pack_type, void *pack_val_var ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack");
    SOS_buffer *byte_buffer;
    SOS_data   *data;
    int         pos;

    SOS_val     pack_val;

    switch(pack_type) {
    case SOS_VAL_TYPE_INT:    pack_val.i_val = *(int *)pack_val_var; break;
    case SOS_VAL_TYPE_LONG:   pack_val.l_val = *(long *)pack_val_var; break;
    case SOS_VAL_TYPE_DOUBLE: pack_val.d_val = *(double *)pack_val_var; break;
    case SOS_VAL_TYPE_STRING: pack_val.c_val = (char *)pack_val_var; break;
    case SOS_VAL_TYPE_BYTES:  pack_val.bytes = (SOS_buffer *)pack_val_var; break;
    default:
        dlog(0, "ERROR: Invalid pack_type sent to SOS_pack.   (%d)\n", (int) pack_type);
        return -1;
        break;
    }


    pthread_mutex_lock(pub->lock);

    /* The hash table will return NULL if a value is not present.
     * The pub->data[elem] index is zero-indexed, so indices are stored +1, to
     *   differentiate between empty and the first position.  The value
     *   returned by SOS_pub_search() is the actual array index to be used.
     */
    pos = SOS_pub_search(pub, name);

    if (pos < 0) {
        /* Value does NOT EXIST in the pub. */
        /* Check if we need to expand the pub */
        if (pub->elem_count >= pub->elem_max) {
            SOS_expand_data(pub);
        }

        // Force a pub announce.
        pub->announced = 0;

        /* Insert the value... */
        pos = pub->elem_count;
        pub->elem_count++;

        /* NOTE: (pos + 1) is correct, we're storing it's "N'th element" position
         *       rather than it's array index. See SOS_pub_search(...) for explanation. */
        pub->name_table->put(pub->name_table, name, (void *) ((long)(pos + 1)));

        data = pub->data[pos];

        data->type  = pack_type;
        data->guid  = SOS_uid_next(SOS->uid.my_guid_pool);
        data->val.c_val = NULL;
        data->val_len = 0;
        strncpy(data->name, name, SOS_DEFAULT_STRING_LEN);

    } else {
        /* Name ALREADY EXISTS in the pub... */
        data = pub->data[pos];
    }

    /* Update the value in the pub->data[elem] position. */
    switch(data->type) {
        
    case SOS_VAL_TYPE_STRING:
        if (data->val.c_val != NULL) {
            free(data->val.c_val);
        }
        if (pack_val.c_val != NULL) {
            data->val.c_val = strndup(pack_val.c_val, SOS_DEFAULT_STRING_LEN);
            data->val_len   = strlen(pack_val.c_val);
        } else {
            dlog(0, "WARNING: You packed a null value for pub(%s)->data[%d]!\n",
                 pub->title, pos);
            data->val.c_val = (char *) malloc(1 * sizeof(unsigned char));
            *data->val.c_val = '\0';
            data->val_len = 0;
        }
        break;

    case SOS_VAL_TYPE_BYTES:
        byte_buffer = (SOS_buffer *) pack_val.bytes;
        if (byte_buffer != NULL) {
            if (data->val.bytes != NULL) { free(data->val.bytes); }
            data->val.bytes = (void *) malloc((1 + byte_buffer->len) * sizeof(unsigned char));
            memcpy(data->val.bytes, byte_buffer->data, byte_buffer->len);
            data->val_len = byte_buffer->len;
        } else {
            dlog(0, "WARNING: You packed a null (SOS_buffer *) for pub(%s)->data[%d]!\n",
                 pub->title, pos);
            SOS_buffer_init_sized(SOS, (SOS_buffer **) &data->val.bytes, SOS_DEFAULT_BUFFER_MIN);
            data->val_len = SOS_DEFAULT_BUFFER_MIN;
        }
        break;
        
    case SOS_VAL_TYPE_INT:
        data->val.i_val = pack_val.i_val;
        data->val_len   = sizeof(int);
        break;

    case SOS_VAL_TYPE_LONG:
        data->val.l_val = pack_val.l_val;
        data->val_len   = sizeof(long);
        break;

    case SOS_VAL_TYPE_DOUBLE:
        data->val.d_val = pack_val.d_val;
        data->val_len   = sizeof(double);
        break;

    default:
        dlog(0, "ERROR: Invalid data type was specified.   (%d)\n", data->type);
        exit(EXIT_FAILURE);

    }

    data->state = SOS_VAL_STATE_DIRTY;
    SOS_TIME( data->time.pack );

    /* Place the packed value into the snap_queue... */
    SOS_val_snap *snap;
    snap = (SOS_val_snap *) malloc(sizeof(SOS_val_snap));
    
    snap->elem     = pos;
    snap->guid     = data->guid;
    snap->mood     = data->meta.mood;
    snap->semantic = data->meta.semantic;
    snap->type     = data->type;
    snap->time     = data->time;
    snap->frame    = pub->frame;

    snap->val_len = data->val_len;
    
    switch(snap->type) {
    case SOS_VAL_TYPE_BYTES:
        snap->val.bytes = (unsigned char *) malloc(snap->val_len * sizeof(unsigned char));
        memcpy(data->val.bytes, snap->val.bytes, snap->val_len);
        break;

    case SOS_VAL_TYPE_STRING:
        snap->val.c_val = strndup(data->val.c_val, SOS_DEFAULT_STRING_LEN);
        break;

    case SOS_VAL_TYPE_INT:
    case SOS_VAL_TYPE_LONG:
    case SOS_VAL_TYPE_DOUBLE:
        snap->val = data->val;
        break;

    default:
        dlog(0, "Invalid data->type at pos == %d   (%d)\n", pos, snap->type);
        exit(EXIT_FAILURE);
    }
    
    pthread_mutex_lock(pub->snap_queue->sync_lock);
    pipe_push(pub->snap_queue->intake, (void *) &snap, 1);
    pub->snap_queue->elem_count++;
    pthread_mutex_unlock(pub->snap_queue->sync_lock);

    /* Done. */

    pthread_mutex_unlock(pub->lock);

    return pos;
}

int SOS_pub_search(SOS_pub *pub, const char *name) {
    long i;
    
    i = (long) pub->name_table->get(pub->name_table, name);

    /* If the name does not exist, i==0... since the data elements are
     * zero-indexed, we subtract 1 from i so that 'does not exist' is
     * returned as -1. This is accounted for when values are initially
     * packed, as the name is added to the name_table with the index being
     * (i + 1)'ed.
     */

    i--;

    return i;
}


void SOS_pub_destroy(SOS_pub *pub) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pub_destroy");
    int elem;

    if (SOS->config.offline_test_mode != true) {
        /* TODO: { PUB DESTROY } Right now this only works in offline test mode
         * within the client-side library code. The Daemon likely has additional
         * memory structures in play.
         *  ...is this still the case?  */
        return;
    }

    if (pub == NULL) { return; }

    pthread_mutex_lock(pub->lock);

    dlog(6, "Freeing pub components:\n");
    dlog(6, "  ... snapshot queue\n");
    pthread_mutex_lock(pub->snap_queue->sync_lock);
    pthread_mutex_destroy(pub->snap_queue->sync_lock);
    pipe_producer_free(pub->snap_queue->intake);
    pipe_consumer_free(pub->snap_queue->outlet);
    dlog(6, "  ... element data: ");
    for (elem = 0; elem < pub->elem_max; elem++) {
        if (pub->data[elem]->type == SOS_VAL_TYPE_STRING) {
            if (pub->data[elem]->val.c_val != NULL) {
                 free(pub->data[elem]->val.c_val);
            }

        }
        if (pub->data[elem] != NULL) { free(pub->data[elem]); }
    }
    dlog(6, "done. (%d element capacity)\n", pub->elem_max);
    dlog(6, "  ... element pointer array\n");
    if (pub->data != NULL) { free(pub->data); }
    dlog(6, "  ... name table\n");
    pub->name_table->free(pub->name_table);
    dlog(6, "  ... lock\n");
    pthread_mutex_destroy(pub->lock);
    dlog(6, "  ... pub handle itself\n");
    if (pub != NULL) { free(pub); }
    dlog(6, "  done.\n");

    return;
}



void SOS_display_pub(SOS_pub *pub, FILE *output_to) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_display_pub");
    
    /* TODO:{ DISPLAY_PUB, CHAD }
     *
     * This needs to get cleaned up and restored to a the useful CSV/TSV that it was.
     */
    
    return;
}



void SOS_val_snap_queue_to_buffer(SOS_pub *pub, SOS_buffer *buffer, bool destroy_snaps) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_val_snap_queue_to_buffer");
    SOS_msg_header header;
    SOS_val_snap *snap;
    char pack_fmt[SOS_DEFAULT_STRING_LEN] = {0};
    int offset;
    int count;

    pthread_mutex_lock(pub->lock);
    pthread_mutex_lock(pub->snap_queue->sync_lock);

    if (pub->snap_queue->elem_count < 1) {
        dlog(4, "  ... nothing to do for pub(%s)\n", pub->guid_str);
        pthread_mutex_unlock(pub->snap_queue->sync_lock);
        pthread_mutex_unlock(pub->lock);
        return;
    }

    int snap_index = 0;
    int snap_count = pub->snap_queue->elem_count;
    SOS_val_snap **snap_list;

    dlog(6, "  ... attempting to pop %d snaps off the queue.\n", snap_count);

    snap_list = (SOS_val_snap **) malloc(snap_count * sizeof(SOS_val_snap *));
    count = pipe_pop(pub->snap_queue->outlet, (void *) snap_list, snap_count);
    pub->snap_queue->elem_count -= count;
    pthread_mutex_unlock(pub->snap_queue->sync_lock);

    if (count != snap_count) {
        dlog(6, "  ... received %d snaps instead!\n", count);
        snap_count = count;
    }

    dlog(6, "  ... building buffer from the val_snap queue:\n");
    dlog(6, "     ... processing header\n");

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_VAL_SNAPS;
    header.msg_from = SOS->my_guid;
    header.pub_guid = pub->guid;

    offset = 0;
    SOS_buffer_pack(buffer, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.pub_guid);

    SOS_buffer_pack(buffer, &offset, "i", snap_count);

    dlog(6, "     ... processing snaps extracted from the queue\n");

    for (snap_index = 0; snap_index < snap_count; snap_index++) {
        snap = snap_list[snap_index];

        dlog(6, "     ... guid=%" SOS_GUID_FMT "\n", snap->guid);

        SOS_TIME(snap->time.send);

        SOS_buffer_pack(buffer, &offset, "igiiiidddl",
                        snap->elem,
                        snap->guid,
                        snap->mood,
                        snap->semantic,
                        snap->type,
                        snap->val_len,
                        snap->time.pack,
                        snap->time.send,
                        snap->time.recv,
                        snap->frame);

        switch (pub->data[snap->elem]->type) {
        case SOS_VAL_TYPE_INT:    SOS_buffer_pack(buffer, &offset, "i", snap->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   SOS_buffer_pack(buffer, &offset, "l", snap->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: SOS_buffer_pack(buffer, &offset, "d", snap->val.d_val); break;
        case SOS_VAL_TYPE_STRING: SOS_buffer_pack(buffer, &offset, "s", snap->val.c_val); break;
        case SOS_VAL_TYPE_BYTES:
            snprintf(pack_fmt, SOS_DEFAULT_STRING_LEN, "%db", snap->val_len);
            SOS_buffer_pack(buffer, &offset, pack_fmt, snap->val.bytes);
        default:
            dlog(0, "ERROR: Invalid type (%d) at index %d of pub->guid == %" SOS_GUID_FMT ".\n", pub->data[snap->elem]->type, snap->elem, pub->guid);
            break;
        }//switch

        if (destroy_snaps == true) {
            if (pub->data[snap->elem]->type == SOS_VAL_TYPE_STRING) { free(snap->val.c_val); }
            if (pub->data[snap->elem]->type == SOS_VAL_TYPE_BYTES) { free(snap->val.bytes); }
            free(snap);
        }

    }//for

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(buffer, &offset, "i", header.msg_size);
    dlog(6, "     ... done   (buf_len == %d)\n", header.msg_size);

    pthread_mutex_unlock(pub->lock);
   
    return;
}



void SOS_val_snap_queue_from_buffer(SOS_buffer *buffer, SOS_pipe *snap_queue, SOS_pub *pub) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_val_snap_queue_from_buffer");
    SOS_msg_header header;
    char           unpack_fmt[SOS_DEFAULT_STRING_LEN] = {0};
    int            offset;
    int            string_len;

    if (pub == NULL) {
        dlog(1, "WARNING! Attempting to build snap_queue for a pub we don't know about.\n");
        dlog(1, "  ... skipping this request.\n");
        return;
    }

    if (snap_queue == NULL) {
        dlog(0, "ERROR: snap_queue == NULL!\n");
        return;
    }

    dlog(6, "  ... building val_snap queue from a buffer:\n");
    dlog(6, "     ... processing header\n");

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);

    dlog(6, "     ... header.msg_size == %d\n", header.msg_size);
    dlog(6, "     ... header.msg_type == %d\n", header.msg_type);
    dlog(6, "     ... header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(6, "     ... header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);

    int snap_index = 0;
    int snap_count = 0;
    SOS_buffer_unpack(buffer, &offset, "i", &snap_count);

    if (snap_count < 1) {
      dlog(1, "WARNING: Attempted to process buffer with ZERO val_snaps.  This is unusual.\n");
      dlog(1, "WARNING:    ... since there is no work to do, returning.\n");
      return;
    }

    SOS_val_snap *snap;
    SOS_val_snap **snap_list;
    snap_list = (SOS_val_snap **) calloc(snap_count, sizeof(SOS_val_snap *));

    for (snap_index = 0; snap_index < snap_count; snap_index++) {
        snap_list[snap_index] = (SOS_val_snap *) calloc(1, sizeof(SOS_val_snap));
        snap = snap_list[snap_index];

        SOS_buffer_unpack(buffer, &offset, "igiiiidddl",
                          &snap->elem,
                          &snap->guid,
                          &snap->mood,
                          &snap->semantic,
                          &snap->type,
                          &snap->val_len,
                          &snap->time.pack,
                          &snap->time.send,
                          &snap->time.recv,
                          &snap->frame);

        dlog(6, "    ... grabbing element[%d] @ %d/%d(%d) -> type == %d, val_len == %d\n", snap->elem, offset, header.msg_size, buffer->len, snap->type, snap->val_len);

        switch (snap->type) {
        case SOS_VAL_TYPE_INT:    SOS_buffer_unpack(buffer, &offset, "i", &snap->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   SOS_buffer_unpack(buffer, &offset, "l", &snap->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: SOS_buffer_unpack(buffer, &offset, "d", &snap->val.d_val); break;
        case SOS_VAL_TYPE_STRING:
            snap->val.c_val = (char *) malloc (snap->val_len * sizeof(char));
            SOS_buffer_unpack(buffer, &offset, "s", snap->val.c_val);
            break;
        case SOS_VAL_TYPE_BYTES:
            memset(unpack_fmt, '\0', SOS_DEFAULT_STRING_LEN);
            offset -= SOS_buffer_unpack(buffer, &offset, "i", &string_len);
            snap->val_len = string_len;
            snap->val.bytes = (void *) malloc((1 + string_len) * sizeof(unsigned char));
            snprintf(unpack_fmt, SOS_DEFAULT_STRING_LEN, "%db", string_len);   //No really needed got unpacking, since the length is already in there...
            SOS_buffer_unpack(buffer, &offset, unpack_fmt, snap->val.bytes);
            break;
        default:
          dlog(6, "ERROR: Invalid type (%d) at index %d with pub->guid == %" SOS_GUID_FMT ".\n", snap->type, snap->elem, pub->guid);
            break;
        }

    }//loop


    dlog(6, "     ... pushing %d snaps down onto the queue.\n", snap_count);
    pthread_mutex_lock(snap_queue->sync_lock);
    pipe_push(snap_queue->intake, (void *) snap_list, snap_count);
    snap_queue->elem_count += snap_count;
    pthread_mutex_unlock( snap_queue->sync_lock );

    free(snap_list);

    dlog(6, "     ... done\n");

    return;
}



void SOS_announce_to_buffer(SOS_pub *pub, SOS_buffer *buffer) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce_to_buffer");
    SOS_msg_header header;
    int   offset;
    int   elem;

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ANNOUNCE;
    header.msg_from = SOS->my_guid;
    header.pub_guid = pub->guid;

    offset = 0;
    SOS_buffer_pack(buffer, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.pub_guid);

    pthread_mutex_lock(pub->lock);

    /* Pub metadata. */
    SOS_buffer_pack(buffer, &offset, "siiississiiiiiii",
                    pub->node_id,
                    pub->process_id,
                    pub->thread_id,
                    pub->comm_rank,
                    pub->prog_name,
                    pub->prog_ver,
                    pub->pragma_len,
                    pub->pragma_msg,
                    pub->title,
                    pub->elem_count,
                    pub->meta.channel,
                    pub->meta.layer,
                    pub->meta.nature,
                    pub->meta.pri_hint,
                    pub->meta.scope_hint,
                    pub->meta.retain_hint);

    /* Data definitions. */
    for (elem = 0; elem < pub->elem_count; elem++) {
        SOS_buffer_pack(buffer, &offset, "gsiiiiiii",
                        pub->data[elem]->guid,
                        pub->data[elem]->name,
                        pub->data[elem]->type,
                        pub->data[elem]->meta.freq,
                        pub->data[elem]->meta.semantic,
                        pub->data[elem]->meta.classifier,
                        pub->data[elem]->meta.pattern,
                        pub->data[elem]->meta.compare,
                        pub->data[elem]->meta.mood );
    }

    /* TODO: { PUB } Come up with better ENUM for announce status. */
    pub->announced = 1;

    pthread_mutex_unlock(pub->lock);

    /* Re-pack the message size now that we know it. */
    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(buffer, &offset, "i", header.msg_size);

    return;
}


void SOS_publish_to_buffer(SOS_pub *pub, SOS_buffer *buffer) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish_to_buffer");
    SOS_msg_header   header;
    long             this_frame;
    double           send_time;
    int              offset;
    int              elem;    


    pthread_mutex_lock(pub->lock);

    /* TODO: { ASYNC CLIENT, TIME }
     *       If client-side gets async send, this wont be true.
     *       Until then, buffer-create time IS the send time.
     */
    SOS_TIME( send_time );

    if (SOS->role == SOS_ROLE_CLIENT) {
        /* Only CLIENT updates the frame when sending, in case this is re-used
         * internally / on the backplane by the LISTENER / AGGREGATOR. */
        this_frame = pub->frame++;
    }

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_PUBLISH;
    header.msg_from = SOS->my_guid;
    header.pub_guid = pub->guid;

    offset = 0;
    SOS_buffer_pack(buffer, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.pub_guid);

    /* Pack in the frame of these elements: */
    SOS_buffer_pack(buffer, &offset, "l", this_frame);

    /* Pack in the data elements. */
    for (elem = 0; elem < pub->elem_count; elem++) {
        if ( pub->data[elem]->state != SOS_VAL_STATE_DIRTY) { continue; }
        
        pub->data[elem]->state = SOS_VAL_STATE_CLEAN;
        pub->data[elem]->time.send = send_time;

        dlog(7, "pub->data[%d]->time.pack == %lf"
             "   pub->data[%d]->time.send == %lf\n",
             elem, pub->data[elem]->time.pack,
             elem, pub->data[elem]->time.send);

        SOS_buffer_pack(buffer, &offset, "iddill",
                        elem,
                        pub->data[elem]->time.pack,
                        pub->data[elem]->time.send,
                        pub->data[elem]->val_len,
                        pub->data[elem]->meta.semantic,
                        pub->data[elem]->meta.mood);

        switch (pub->data[elem]->type) {
        case SOS_VAL_TYPE_INT:     SOS_buffer_pack(buffer, &offset, "i", pub->data[elem]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:    SOS_buffer_pack(buffer, &offset, "l", pub->data[elem]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE:  SOS_buffer_pack(buffer, &offset, "d", pub->data[elem]->val.d_val); break;
        case SOS_VAL_TYPE_STRING:  SOS_buffer_pack(buffer, &offset, "s", pub->data[elem]->val.c_val);
            dlog(8, "[STRING]: Packing in -> \"%s\" ...\n", pub->data[elem]->val.c_val);
            break;
        case SOS_VAL_TYPE_BYTES:   SOS_buffer_pack(buffer, &offset, "b", pub->data[elem]->val.bytes); break;
        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid == %" SOS_GUID_FMT ".\n", pub->data[elem]->type, elem, pub->guid);
            break;
        }//switch
    }//for

    /* Re-pack the message size now that we know what it is. */
    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(buffer, &offset, "i", header.msg_size);

    pthread_mutex_unlock(pub->lock);

    return;
}


void SOS_announce_from_buffer(SOS_buffer *buffer, SOS_pub *pub) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce_from_buffer");
    SOS_msg_header header;
    SOS_data       announced_elem;
    int            offset;
    int            elem;

    pthread_mutex_lock(pub->lock);

    dlog(6, "Applying an ANNOUNCE from a buffer...\n");
    dlog(6, "  ... unpacking the header.\n");

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
                      &header.msg_size,
                      &header.msg_type,
                      &header.msg_from,
                      &header.pub_guid);

    pub->guid = header.pub_guid;
    snprintf(pub->guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, pub->guid);

    dlog(6, "  ... unpacking the pub definition.\n");
    SOS_buffer_unpack(buffer, &offset, "siiississiiiiiii",
         pub->node_id,
        &pub->process_id,
        &pub->thread_id,
        &pub->comm_rank,
         pub->prog_name,
         pub->prog_ver,
        &pub->pragma_len,
         pub->pragma_msg,
         pub->title,
        &elem,
        &pub->meta.channel,
        &pub->meta.layer,
        &pub->meta.nature,
        &pub->meta.pri_hint,
        &pub->meta.scope_hint,
        &pub->meta.retain_hint);

    dlog(6, "pub->node_id = \"%s\"\n", pub->node_id);
    dlog(6, "pub->process_id = %d\n", pub->process_id);
    dlog(6, "pub->thread_id = %d\n", pub->thread_id);
    dlog(6, "pub->comm_rank = %d\n", pub->comm_rank);
    dlog(6, "pub->prog_name = \"%s\"\n", pub->prog_name);
    dlog(6, "pub->prog_ver = \"%s\"\n", pub->prog_ver);
    dlog(6, "pub->pragma_len = %d\n", pub->pragma_len);
    dlog(6, "pub->pragma_msg = \"%s\"\n", pub->pragma_msg);
    dlog(6, "pub->title = \"%s\"\n", pub->title);
    dlog(6, "pub->elem_count = %d\n", elem);
    dlog(6, "pub->meta.channel = %d\n", pub->meta.channel);
    dlog(6, "pub->meta.layer = %d\n", pub->meta.layer);
    dlog(6, "pub->meta.nature = %d\n", pub->meta.nature);
    dlog(6, "pub->meta.pri_hint = %d\n", pub->meta.pri_hint);
    dlog(6, "pub->meta.scope_hint = %d\n", pub->meta.scope_hint);
    dlog(6, "pub->meta.retain_hint = %d\n", pub->meta.retain_hint);

    if ((SOS->role != SOS_ROLE_CLIENT)) {
        dlog(1, "AUTOGROW --\n");
        dlog(1, "AUTOGROW --\n");
        dlog(1, "AUTOGROW -- Announced pub elem_count: %d\n", elem);
        dlog(1, "AUTOGROW -- In-memory pub elem_count: %d\n", pub->elem_max);
        dlog(1, "AUTOGROW --\n");
        dlog(1, "AUTOGROW --\n");
    }

    /* Ensure there is room in this pub to handle incoming data definitions. */
    while(pub->elem_max < elem) {
        dlog(6, "  ... growing pub->elem_max from %d to handle %d elements...\n", pub->elem_max, elem);
        SOS_expand_data(pub);
    }
    pub->elem_count = elem;

    dlog(6, "  ... unpacking the data definitions.\n");
    /* Unpack the data definitions: */
    elem = 0;
    for (elem = 0; elem < pub->elem_count; elem++) {
        memset(&announced_elem, 0, sizeof(SOS_data));

        SOS_buffer_unpack(buffer, &offset, "gsiiiiiii",
            &announced_elem.guid,
            announced_elem.name,
            &announced_elem.type,
            &announced_elem.meta.freq,
            &announced_elem.meta.semantic,
            &announced_elem.meta.classifier,
            &announced_elem.meta.pattern,
            &announced_elem.meta.compare,
            &announced_elem.meta.mood );

        if ((    pub->data[elem]->guid            != announced_elem.guid )
            || ( pub->data[elem]->type            != announced_elem.type )
            || ( pub->data[elem]->meta.freq       != announced_elem.meta.freq )
            || ( pub->data[elem]->meta.semantic   != announced_elem.meta.semantic )
            || ( pub->data[elem]->meta.classifier != announced_elem.meta.classifier )
            || ( pub->data[elem]->meta.pattern    != announced_elem.meta.pattern )
            || ( pub->data[elem]->meta.compare    != announced_elem.meta.compare )
            || ( pub->data[elem]->meta.mood       != announced_elem.meta.mood )) {
        
            /* This is a value we have not seen before, or that has
             * changed.  Update the fields and set the sync flag to
             * _RENEW so it gets stored in the database.
             */

            pub->data[elem]->sync = SOS_VAL_SYNC_RENEW;

            /* Set the element name. */
            snprintf(pub->data[elem]->name, SOS_DEFAULT_STRING_LEN, "%s", announced_elem.name);
            /* Store the name/position pair in the name_table. */
            pub->name_table->put(pub->name_table, pub->data[elem]->name, (void *) ((long)(elem + 1)));

            pub->data[elem]->guid            = announced_elem.guid;
            pub->data[elem]->type            = announced_elem.type;
            pub->data[elem]->meta.freq       = announced_elem.meta.freq;
            pub->data[elem]->meta.semantic   = announced_elem.meta.semantic;
            pub->data[elem]->meta.classifier = announced_elem.meta.classifier;
            pub->data[elem]->meta.pattern    = announced_elem.meta.pattern;
            pub->data[elem]->meta.compare    = announced_elem.meta.compare;
            pub->data[elem]->meta.mood       = announced_elem.meta.mood;

            dlog(6, "  ... pub->data[%d]->guid = %" SOS_GUID_FMT "\n", elem, pub->data[elem]->guid);
            dlog(6, "  ... pub->data[%d]->name = %s\n", elem, pub->data[elem]->name);
            dlog(6, "  ... pub->data[%d]->type = %d\n", elem, pub->data[elem]->type);
            
        } else {
            /* This pub is being re-announcd, but we already have
             * identical data for this value.  Don't replace it or
             * change its sync status, as already exists in the db.
             */
            continue;
        }

    }//for

    pthread_mutex_unlock(pub->lock);
    dlog(6, "  ... done.\n");

    return;
}

void SOS_publish_from_buffer(SOS_buffer *buffer, SOS_pub *pub, SOS_pipe *snap_queue) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish_from_buffer");
    SOS_msg_header  header;
    SOS_data       *data;
    long            this_frame;
    int             offset;
    int             elem;

    if (buffer == NULL) {
        dlog(0, "ERROR: SOS_buffer *buffer parameter is NULL!  Terminating.\n");
        exit(EXIT_FAILURE);
    }

    if (pub == NULL) {
        dlog(0, "ERROR: SOS_pub *pub parameter is NULL!  Terminating.\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(pub->lock);

    dlog(7, "Unpacking the values from the buffer...\n");

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    dlog(7, "  ... header.msg_size = %d\n", header.msg_size);
    dlog(7, "  ... header.msg_type = %d\n", header.msg_type);
    dlog(7, "  ... header.msg_from = %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(7, "  ... header.pub_guid = %" SOS_GUID_FMT "\n", header.pub_guid);
    dlog(7, "  ... values:\n");

    SOS_buffer_unpack(buffer, &offset, "l", &this_frame);
    pub->frame = this_frame;

    /* Unpack in the data elements. */
    while (offset < header.msg_size) {
        dlog(7, "Unpacking next message @ offset %d of %d...\n", offset, header.msg_size);

        SOS_buffer_unpack(buffer, &offset, "i", &elem);
        data = pub->data[elem];

        SOS_buffer_unpack(buffer, &offset, "ddill",
                          &data->time.pack,
                          &data->time.send,
                          &data->val_len,
                          &data->meta.semantic,
                          &data->meta.mood);

        dlog(7, "pub->data[%d]->time.pack == %lf   pub->data[%d]->time.send == %lf\n",
             elem, data->time.pack,
             elem, data->time.send);

        switch (data->type) {
        case SOS_VAL_TYPE_INT:    SOS_buffer_unpack(buffer, &offset, "i", &data->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   SOS_buffer_unpack(buffer, &offset, "l", &data->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: SOS_buffer_unpack(buffer, &offset, "d", &data->val.d_val); break;
        case SOS_VAL_TYPE_STRING:
            if (data->val.c_val != NULL) {
                free(data->val.c_val );
            }
            data->val.c_val = (char *) malloc(1 + data->val_len);
            memset(data->val.c_val, '\0', (1 + data->val_len));
            SOS_buffer_unpack(buffer, &offset, "s", data->val.c_val);
            dlog(8, "[STRING] Extracted pub message string: %s\n", data->val.c_val);
            break;
        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid == %" SOS_GUID_FMT ".\n", data->type, elem, pub->guid);
            break;
        }

        data->state = SOS_VAL_STATE_CLEAN;

        /* NOTE: ALL values go into the snap queue, so this need not happen anymore.
         *
         * Enqueue this value for writing out to local_sync and cloud_sync.
        if (snap_queue != NULL) {
            dlog(7, "Enqueing a val_snap for \"%s\"\n", pub->data[elem]->name);
            SOS_val_snap *snap;
            snap = (SOS_val_snap *) malloc(sizeof(SOS_val_snap));

            snap->elem     = elem;
            snap->guid     = data->guid;
            snap->mood     = data->meta.mood;
            snap->semantic = data->meta.semantic;
            snap->type     = data->type;
            snap->val      = data->val;    // Take over extant string.
            snap->val_len  = data->val_len;
            snap->time     = data->time;
            snap->frame    = pub->frame;

            pthread_mutex_lock(snap_queue->sync_lock);
            pipe_push(snap_queue->intake, (void *) &snap, 1);
            snap_queue->elem_count++;
            pthread_mutex_unlock(snap_queue->sync_lock);
        }//if
        */

    }//while

    pthread_mutex_unlock(pub->lock);
    dlog(7, "  ... done.\n");

    return;
}


void SOS_announce( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce");
    SOS_buffer *ann_buf;
    SOS_buffer *rep_buf;

    SOS_buffer_init(SOS, &ann_buf);
    SOS_buffer_init_sized(SOS, &rep_buf, SOS_DEFAULT_REPLY_LEN);

    dlog(6, "Preparing an announcement message...\n");

    if (pub->announced != 0) {
        dlog(0, "WARNING: This publication has already been announced!  Doing nothing.\n");
        return;
    }

    dlog(6, "  ... placing the announce message in a buffer.\n");
    SOS_announce_to_buffer(pub, ann_buf);
    dlog(6, "  ... sending the buffer to the daemon.\n");
    SOS_send_to_daemon(ann_buf, rep_buf);
    dlog(6, "  ... done.\n");

    SOS_buffer_destroy(ann_buf);
    SOS_buffer_destroy(rep_buf);

    return;
}


void SOS_publish( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish");
    SOS_buffer *pub_buf;
    SOS_buffer *rep_buf;
    SOS_buffer_init(SOS, &pub_buf);
    SOS_buffer_init_sized(SOS, &rep_buf, SOS_DEFAULT_REPLY_LEN);

    if (pub->announced == 0) {
        dlog(6, "AUTO-ANNOUNCING this pub...\n");
        SOS_announce(pub);
    }

    dlog(6, "Preparing a publish message...\n");
    dlog(6, "  ... placing the publish message in a buffer.\n");
    SOS_publish_to_buffer(pub, pub_buf);
    dlog(6, "  ... sending the buffer to the daemon.\n");
    SOS_send_to_daemon(pub_buf, rep_buf);

    dlog(6, "  ... checking for val_snap queue entries.\n");
    SOS_buffer_wipe(pub_buf);
    SOS_val_snap_queue_to_buffer(pub, pub_buf, true);
    if (pub_buf->len > 0) {
        dlog(6, "  ... sending the buffer to the daemon.\n");
        SOS_send_to_daemon(pub_buf, rep_buf);
    }
    dlog(6, "  ... done.\n");

    SOS_buffer_destroy(pub_buf);
    SOS_buffer_destroy(rep_buf);

    return;
}


