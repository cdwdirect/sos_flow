
/**
 * @file sos.c
 * @author Chad Wood
 * @brief Core SOS library routines
 *
 * These routines provide the essential functionality of SOS
 * and are used by both the client library as well as the
 * daemons.
 *
 * All of the custom types and enumerations used by SOS are
 * defined in the @c sos_types.h file, as well as the
 * @c sos_buffer.h @c sos_pipe.h and @c sos_qhashtbl.h files.
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
#include <fcntl.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_debug.h"
#include "sos_buffer.h"
#include "sos_pipe.h"
#include "sos_qhashtbl.h"

// Private functions (not in the header file)

/**
 * @brief Check for feedback from the daemon on a regular heartbeat.
 */
void*        SOS_THREAD_receives_timed(void *sos_runtime_ptr);


/**
 * @brief Open a socket and actively listen for feedback messages.
 */
void*        SOS_THREAD_receives_direct(void *sos_runtime_ptr);

/**
 * @brief Process the feedback messages, by whatever means they came in.
 */
void SOS_process_feedback(SOS_buffer *buffer);

/**
 * @brief An internal utility function for growing a pub to hold more data.
 */
void         SOS_expand_data(SOS_pub *pub);

/**
 * @brief Launch a thread for feedback handling specified by the user.
 */
void SOS_receiver_init(SOS_runtime *sos_context);

/**
 * @brief Internal utility function to see if a file exists.
 * @return 1 == file exists, 0 == file does not exist.
 */
int SOS_file_exists(char *filepath) {
    struct stat   buffer;   
    return (stat(filepath, &buffer) == 0);
}

/**
 * @brief Initialize the SOS library and register with the SOS runtime.
 *
 * This is the first SOS function that gets called. If the client
 * application is an MPI application, the SOS_init function is
 * traditionally invoked immediately following MPI_Init.
 *
 * Users do not need to allocate memory for the @p sos_runtime variable,
 * when the address of a pointer is passed in, the SOS library will
 * allocate all of the memory needed to populate it. This runtime is
 * then passed to SOS object creation functions to connect the various
 * runtime elements together into a unified context that shares metadata
 * and uses a common block of GUID values.
 *
 * If this SOS client is not interested in receiving or processing
 * feedback from the SOS runtime or analytics modules, the
 * @p receives parameter should be set to SOS_RECEIVES_NO_FEEDBACK
 * and the @p handler can be set to NULL.
 *
 * @param argc The address of the argc variable from main, or NULL.
 * @param argv The address of the argv variable from main, or NULL.
 * @param sos_runtime The address of an uninitialized SOS_runtime pointer.
 * @param role What this client is doing, e.g: @c SOS_ROLE_CLIENT
 * @param receives What feedback is expected, e.g: @c SOS_RECEIVES_NO_FEEDBACK
 * @param handler Function pointer to a user-defined feedback handler.
 * @warning The SOS daemon needs to be up and running before calling.
 */
void
SOS_init(int *argc, char ***argv, SOS_runtime **sos_runtime,
    SOS_role role, SOS_receives receives, SOS_feedback_handler_f handler)
{
    *sos_runtime = (SOS_runtime *) malloc(sizeof(SOS_runtime));
     memset(*sos_runtime, '\0', sizeof(SOS_runtime));
    SOS_init_existing_runtime(argc, argv, sos_runtime, role, receives, handler);
    return;
}

/**
 * @brief Initialize the SOS library using an existing context handle.
 *
 * This function is used when special circumstances (like the SOS daemon)
 * require a complex chain of initialization behaviors using parts of
 * the runtime context prior to it being fully initialized. Use this
 * function only if you have already allocated memory for the SOS_runtime
 * object.
 *
 * Most users will call the @c SOS_init(...) function instead of this,
 * as sending a pointer in that does not reference the correct amount
 * of memory that is owned by this process ( @c sizeof(SOS_runtime) )
 * can trigger a segmentation fault error.
 *
 * The normal @c SOS_init(...) routine handles the allocation of memory
 * for the runtime object and then calls this function.
 *
 * @param argc The address of the argc variable from main, or NULL.
 * @param argv The address of the argv variable from main, or NULL.
 * @param sos_runtime The address of an uninitialized SOS_runtime pointer.
 * @param role What this client is doing, e.g: @c SOS_ROLE_CLIENT
 * @param receives What feedback is expected, e.g: @c SOS_RECEIVES_NO_FEEDBACK
 * @param handler Function pointer to a user-defined feedback handler.
 * @warning The SOS daemon needs to be up and running before calling.
 */
void
SOS_init_existing_runtime(int *argc, char ***argv, SOS_runtime **sos_runtime,
    SOS_role role, SOS_receives receives, SOS_feedback_handler_f handler)
{
    SOS_msg_header header;
    int i, n, retval, server_socket_fd;
    SOS_guid guid_pool_from;
    SOS_guid guid_pool_to;

    SOS_runtime *NEW_SOS = NULL;
    NEW_SOS = *sos_runtime;

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

    NEW_SOS->net.sos_context = NEW_SOS;

    NEW_SOS->status = SOS_STATUS_INIT;
    NEW_SOS->config.layer = SOS_LAYER_DEFAULT;
    NEW_SOS->config.receives = receives;
    NEW_SOS->config.feedback_handler = handler;
    NEW_SOS->config.receives_port = -1;
    NEW_SOS->config.receives_ready = -1;
    NEW_SOS->config.process_id = (int) getpid();
    SOS_SET_CONTEXT(NEW_SOS, "SOS_init");
    // The SOS_SET_CONTEXT macro makes a new variable, 'SOS'...

    dlog(1, "Initializing SOS ...\n");
    dlog(4, "  ... setting argc / argv\n");

    if (argc == NULL || argv == NULL) {
        dlog(4, "NOTE: argc == NULL || argv == NULL, using"
                " safe but meaningles values.\n");
        SOS->config.argc = 2;
        SOS->config.argv = (char **) malloc(2 * sizeof(char *));
        SOS->config.argv[0] = strdup("[NULL]");
        SOS->config.argv[1] = strdup("[NULL]");
    } else {
        SOS->config.argc = *argc;
        SOS->config.argv = *argv;
    }


    SOS->config.node_id = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    gethostname( SOS->config.node_id, SOS_DEFAULT_STRING_LEN );
    dlog(4, "  ... node_id: %s\n", SOS->config.node_id );

    if (SOS->config.offline_test_mode == true) {
        // Offline mode finishes up non-networking init and bails out.
        SOS_uid_init(SOS, &SOS->uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
        SOS_uid_init(SOS, &SOS->uid.my_guid_pool, 0, SOS_DEFAULT_UID_MAX);
        SOS->my_guid = SOS_uid_next( SOS->uid.my_guid_pool );
        SOS->status = SOS_STATUS_RUNNING;
        dlog(4, "  ... done with SOS_init().  [OFFLINE_TEST_MODE]\n");
        dlog(4, "SOS->status = SOS_STATUS_RUNNING\n");
        return;
    }


    if (SOS->role == SOS_ROLE_CLIENT) {
        // NOTE: This is only used for clients.  Daemons handle their own.
        char *env_rank;
        char *env_size;

        env_rank = getenv("PMI_RANK");
        env_size = getenv("PMI_SIZE");
        if ((env_rank!= NULL) && (env_size != NULL)) {
            // MPICH_
            SOS->config.comm_rank = atoi(env_rank);
            SOS->config.comm_size = atoi(env_size);
            dlog(4, "  ... MPICH environment detected."
                    " (rank: %d/ size:%d)\n",
                    SOS->config.comm_rank,
                    SOS->config.comm_size);
        } else {
            env_rank = getenv("OMPI_COMM_WORLD_RANK");
            env_size = getenv("OMPI_COMM_WORLD_SIZE");
            if ((env_rank != NULL) && (env_size != NULL)) {
                // OpenMPI
                SOS->config.comm_rank = atoi(env_rank);
                SOS->config.comm_size = atoi(env_size);
                dlog(4, "  ... OpenMPI environment detected."
                        " (rank: %d/ size:%d)\n",
                        SOS->config.comm_rank,
                        SOS->config.comm_size);
            } else {
                // non-MPI client.
                SOS->config.comm_rank = 0;
                SOS->config.comm_size = 1;
                dlog(4, "  ... Non-MPI environment detected."
                        " (rank: %d/ size:%d)\n",
                        SOS->config.comm_rank,
                        SOS->config.comm_size);
            }
        }
    }

    if ((SOS->role == SOS_ROLE_CLIENT )
        || (SOS->role == SOS_ROLE_ANALYTICS))
    {
        // 
        //
        //  CLIENT
        //
        //
        dlog(4, "  ... setting up socket communications with the daemon.\n" );

        SOS_buffer_init(SOS, &SOS->net.recv_part);

        SOS->net.send_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        retval = pthread_mutex_init(SOS->net.send_lock, NULL);
        if (retval != 0) {
            fprintf(stderr, " ... ERROR (%d) creating SOS->net.send_lock!"
                "  (%s)\n", retval, strerror(errno));
            free(*sos_runtime);
            *sos_runtime = NULL;
            return;
        }
        SOS->net.buffer_len    = SOS_DEFAULT_BUFFER_MAX;
        SOS->net.timeout       = SOS_DEFAULT_MSG_TIMEOUT;
        strncpy(SOS->net.server_host, SOS_DEFAULT_SERVER_HOST, NI_MAXHOST);
        strncpy(SOS->net.server_port, getenv("SOS_CMD_PORT"), NI_MAXSERV);
        if ((SOS->net.server_port == NULL)
                || (strlen(SOS->net.server_port)) < 2)
        {
            fprintf(stderr, "STATUS: SOS_CMD_PORT evar not set."
                    " Using default: %s\n",
                    SOS_DEFAULT_SERVER_PORT);
            fflush(stderr);
            strncpy(SOS->net.server_port, SOS_DEFAULT_SERVER_PORT, NI_MAXSERV);
        }

        SOS->net.server_hint.ai_family   = AF_UNSPEC;   // Allow IPv4 or IPv6
        SOS->net.server_hint.ai_protocol = 0;           // Any protocol
        SOS->net.server_hint.ai_socktype = SOCK_STREAM; // _STREAM/_DGRAM/_RAW
        SOS->net.server_hint.ai_flags 
            = AI_NUMERICSERV | SOS->net.server_hint.ai_flags;

        retval = getaddrinfo(SOS->net.server_host, SOS->net.server_port,
             &SOS->net.server_hint, &SOS->net.result_list );
        if (retval < 0) {
            fprintf(stderr, "ERROR!  Could not locate the SOS daemon."
                    " (%s:%s)\n",
                    SOS->net.server_host,
                    SOS->net.server_port);
            pthread_mutex_destroy(SOS->net.send_lock);
            free(SOS->net.send_lock);
            free(*sos_runtime);
            *sos_runtime = NULL;
            return;
        }

        for (SOS->net.server_addr = SOS->net.result_list ;
            SOS->net.server_addr != NULL ;
            SOS->net.server_addr = SOS->net.server_addr->ai_next)
        {
            // Iterate the possible connections and register with the SOS daemon:
            server_socket_fd = socket(SOS->net.server_addr->ai_family,
                    SOS->net.server_addr->ai_socktype,
                    SOS->net.server_addr->ai_protocol);
            if (server_socket_fd == -1) { continue; }
            if (connect(server_socket_fd,
                        SOS->net.server_addr->ai_addr,
                        SOS->net.server_addr->ai_addrlen) != -1)
            {
                    // Success!
                    break;
            }
            close(server_socket_fd);
            server_socket_fd = -1;
        } //for (iterate connections)

        freeaddrinfo( SOS->net.result_list );
        
        if (server_socket_fd == 0) {
            fprintf(stderr, "ERROR!  Could not connect to"
                    " sosd.  (%s:%s)\n",
                    SOS->net.server_host, SOS->net.server_port);
            pthread_mutex_destroy(SOS->net.send_lock);
            free(SOS->net.send_lock);
            free(*sos_runtime);
            *sos_runtime = NULL;
            return;
        }

        dlog(4, "  ... registering this instance with SOS->   (%s:%s)\n",
            SOS->net.server_host, SOS->net.server_port);

        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = 0;
        header.ref_guid = 0;

        SOS_buffer *buffer;
        SOS_buffer_init_sized_locking(SOS, &buffer, 1024, false);
        
        int offset = 0;
        SOS_buffer_pack(buffer, &offset, "iigg", 
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.ref_guid);

        //Send client version information:
        SOS_buffer_pack(buffer, &offset, "ii",
            SOS_VERSION_MAJOR,
            SOS_VERSION_MINOR);

        header.msg_size = offset;
        offset = 0;
        SOS_buffer_pack(buffer, &offset, "i", header.msg_size);

        pthread_mutex_lock(SOS->net.send_lock);

        dlog(4, "Built a registration message:\n");
        dlog(4, "  ... buffer->data == %ld\n", (long) buffer->data);
        dlog(4, "  ... buffer->len  == %d\n", buffer->len);
        dlog(4, "Calling send...\n");

        retval = send( server_socket_fd, buffer->data, buffer->len, 0);

        if (retval < 0) {
            fprintf(stderr, "ERROR!  Could not write to server socket!"
                    "  (%s:%s)\n",
                    SOS->net.server_host, SOS->net.server_port);
            pthread_mutex_destroy(SOS->net.send_lock);
            free(SOS->net.send_lock);
            free(*sos_runtime);
            *sos_runtime = NULL;
            return;
        } else {
            dlog(4, "Registration message sent.   (retval == %d)\n", retval);
        }

        SOS_buffer_wipe(buffer);

        dlog(4, "  ... listening for the server to reply...\n");
        buffer->len = recv(server_socket_fd, (void *) buffer->data,
                buffer->max, 0);
        dlog(4, "  ... server responded with %d bytes.\n", retval);

        close( server_socket_fd );
        server_socket_fd = -1;
        pthread_mutex_unlock(SOS->net.send_lock);

        offset = 0;
        SOS_buffer_unpack(buffer, &offset, "iigg",
                &header.msg_size,
                &header.msg_type,
                &header.msg_from,
                &header.ref_guid);
                
        SOS_buffer_unpack(buffer, &offset, "gg",
                &guid_pool_from,
                &guid_pool_to);

        int server_version_major = -1;
        int server_version_minor = -1;

        SOS_buffer_unpack(buffer, &offset, "ii",
                &server_version_major,
                &server_version_minor);

        if ((server_version_major != SOS_VERSION_MAJOR)
            || (server_version_minor != SOS_VERSION_MINOR)) {
            fprintf(stderr, "CRITICAL WARNING: SOS client library (%d.%d) and"
                    " daemon (%d.%d) versions differ!\n",
                    SOS_VERSION_MAJOR,
                    SOS_VERSION_MINOR,
                    server_version_major,
                    server_version_minor);
             fprintf(stderr, "                  ** CLIENT ** Attempting to"
                    " proceed anyway...\n");
            fflush(stderr);
        }

 

        dlog(4, "  ... received guid range from %" SOS_GUID_FMT " to %"
            SOS_GUID_FMT ".\n", guid_pool_from, guid_pool_to);
        dlog(4, "  ... configuring uid sets.\n");

        SOS_uid_init(SOS, &SOS->uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
        SOS_uid_init(SOS, &SOS->uid.my_guid_pool,
            guid_pool_from, guid_pool_to);

        SOS->my_guid = SOS_uid_next(SOS->uid.my_guid_pool);
        dlog(4, "  ... SOS->my_guid == %" SOS_GUID_FMT "\n", SOS->my_guid);

        SOS_buffer_destroy(buffer);


    } else {
         //
         //  CONFIGURATION: LISTENER / AGGREGATOR / etc.
         //
         // NOTE: The daemons handle a lot of their own init
         // Internally, as they have to do some custom stuff.
         // For more, see sosd.c and sosd_cloud_?????.c, the
         // SOSD_init(...) and SOSD_cloud_init(...) functions
         // in particular.
    }

    *sos_runtime = SOS;
    SOS->status = SOS_STATUS_RUNNING;

    dlog(2, "  ... waiting for the feedback receiver thread to come online...\n");
    if (SOS->role == SOS_ROLE_CLIENT) {
        SOS->config.locale = SOS_LOCALE_DEFAULT;

        if (SOS->config.runtime_utility == false) { 
            SOS_receiver_init(SOS);
        }
    }

    if (SOS->config.receives == SOS_RECEIVES_DIRECT_MESSAGES) {
        while(SOS->config.receives_ready != 1) {
            usleep(10000);
        }
    }

    dlog(1, "  ... done with SOS_init().\n");
    dlog(4, "SOS->status = SOS_STATUS_RUNNING\n");
    return;
}



void
SOS_receiver_init(SOS_runtime *sos_context)
{
    SOS_SET_CONTEXT(sos_context, "SOS_receiver_init");
    int retval;

    switch (SOS->config.receives) {

        case SOS_RECEIVES_MANUAL_CHECKIN:
        case SOS_RECEIVES_NO_FEEDBACK:
        case SOS_RECEIVES_DAEMON_MODE:
            return;

        case SOS_RECEIVES_TIMED_CHECKIN:
            dlog(1, "  ... launching libsos runtime thread[s].\n");
            SOS->task.feedback = (pthread_t *) malloc(sizeof(pthread_t));
            SOS->task.feedback_lock
                    = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
            SOS->task.feedback_cond
                    = (pthread_cond_t *)  malloc(sizeof(pthread_cond_t));
            retval = pthread_mutex_init(SOS->task.feedback_lock, NULL);
            if (retval != 0) {
                dlog(0, " ... ERROR (%d) creating SOS->task.feedback_lock!"
                    "  (%s)\n", retval, strerror(errno));
                exit(EXIT_FAILURE);
            }
            retval = pthread_cond_init(SOS->task.feedback_cond, NULL);
            if (retval != 0) {
                dlog(0, " ... ERROR (%d) creating SOS->task.feedback_cond!"
                    "  (%s)\n", retval, strerror(errno));
                exit(EXIT_FAILURE);
            }
            retval = pthread_create(SOS->task.feedback, NULL,
                    SOS_THREAD_receives_timed, (void *) SOS);
            if (retval != 0) {
                dlog(0, " ... ERROR (%d) launching SOS->task.feedback "
                    " thread!  (%s)\n", retval, strerror(errno));
                exit(EXIT_FAILURE);
            }
            break;

        case SOS_RECEIVES_DIRECT_MESSAGES:
            dlog(1, "  ... launching libsos runtime thread[s].\n");
            SOS->task.feedback = (pthread_t *) malloc(sizeof(pthread_t));
            SOS->task.feedback_lock
                    = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
            SOS->task.feedback_cond
                    = (pthread_cond_t *)  malloc(sizeof(pthread_cond_t));
            retval = pthread_mutex_init(SOS->task.feedback_lock, NULL);
            if (retval != 0) {
                dlog(0, " ... ERROR (%d) creating SOS->task.feedback_lock!"
                    "  (%s)\n", retval, strerror(errno));
                exit(EXIT_FAILURE);
            }
            retval = pthread_cond_init(SOS->task.feedback_cond, NULL);
            if (retval != 0) {
                dlog(0, " ... ERROR (%d) creating SOS->task.feedback_cond!"
                    "  (%s)\n", retval, strerror(errno));
                exit(EXIT_FAILURE);
            }
            retval = pthread_create(SOS->task.feedback, NULL,
                SOS_THREAD_receives_direct, (void *) SOS);
            if (retval != 0) {
                dlog(0, " ... ERROR (%d) launching SOS->task.feedback "
                    " thread!  (%s)\n", retval, strerror(errno));
                exit(EXIT_FAILURE);
            }
            break;

        default:
            dlog(0, " ... WARNING: An invalid value was specified for the"
                " feedback receipt mode!  (%d)\n", SOS->config.receives);
            break;
    }//switch

    return;
}


void
SOS_sense_register(SOS_runtime *sos_context, char *handle)
{
    SOS_SET_CONTEXT(sos_context, "SOS_sense_register");
    
    SOS_msg_header header;
    SOS_buffer *msg = NULL;
    SOS_buffer *reply = NULL;
    int offset = 0;

    SOS_buffer_init_sized_locking(SOS, &msg, 1024, false);
    SOS_buffer_init_sized_locking(SOS, &msg, 64, false);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SENSITIVITY;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

    offset = 0;
    SOS_buffer_pack(msg, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.ref_guid);

    SOS_buffer_pack(msg, &offset, "ssi",
            handle,
            SOS->config.node_id,
            SOS->config.receives_port);

    header.msg_size = offset;
    offset = 0;

    SOS_buffer_pack(msg, &offset, "i",
            header.msg_size);

    SOS_send_to_daemon(msg, reply);

    return;
}

void
SOS_sense_trigger(SOS_runtime *sos_context,
    char *handle, char *data, int data_length)
{
    SOS_SET_CONTEXT(sos_context, "SOS_sense_trigger");

    SOS_msg_header header;

    SOS_buffer *msg;
    SOS_buffer *reply;

    SOS_buffer_init_sized_locking(SOS, &msg, SOS_DEFAULT_BUFFER_MAX, false);
    SOS_buffer_init_sized_locking(SOS, &reply, 128, false);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_TRIGGERPULL;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0; 

    int offset = 0;
    SOS_buffer_pack(msg, &offset, "iigg",
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.ref_guid);

    SOS_buffer_pack(msg, &offset, "sis",
            handle,
            data_length,
            data);

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(msg, &offset, "i", header.msg_size);

    SOS_send_to_daemon(msg, reply);

    SOS_buffer_destroy(msg);
    SOS_buffer_destroy(reply);

    return;
}
// ----------



// NOTE: The at_offset lets this function seal messages that are
// embedded inside of an existing buffer. For standalone messages,
// it will be zero.
// Eventually this could be used to compress all but the header of
// the message, but for now it simply goes back and sets the final
// length in the correct part of the header.
int
SOS_msg_zip(
        SOS_buffer *msg,
        int msg_length,
        int at_offset)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOS_message_zip");

    return 0;
}


// NOTE: This exists to allow us to de-compress, in the future.
// For now it doesn't do anything but extract the header and
// the offset that points to the beginning of the data segment.
int
SOS_msg_unzip(
        SOS_buffer *msg,
        SOS_msg_header *header,
        int *offset_after_header)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOS_message_unzip");

    return 0;
}

int
SOS_target_recv_msg(
        SOS_socket_out *target,
        SOS_buffer *reply)
{
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_recv_msg");
    SOS_msg_header header;
    int offset = 0;

    if (SOS->status == SOS_STATUS_SHUTDOWN) {
        dlog(0, "Ignoring receive call because SOS is shutting down.\n");
        return -1;
    }

    int server_socket_fd = target->server_socket_fd;

    if (reply == NULL) {
        dlog(0, "WARNING: Attempting to receive message into uninitialzied"
                " buffer.  Attempting to init/proceed...\n");
        SOS_buffer_init_sized_locking(SOS, &reply,
                SOS_DEFAULT_BUFFER_MAX, false); 
    }

    reply->len = recv(server_socket_fd, reply->data, 
            reply->max, 0);
    if (reply->len < 0) {
        fprintf(stderr, "SOS: recv() call returned an error:\n\t\"%s\"\n",
                strerror(errno));
    }

    memset(&header, '\0', sizeof(SOS_msg_header));
    if (reply->len >= sizeof(SOS_msg_header)) {
        int offset = 0;
        SOS_buffer_unpack(reply, &offset, "iigg",
                &header.msg_size,
                &header.msg_type,
                &header.msg_from,
                &header.ref_guid);
    } else {
        fprintf(stderr, "SOS: Received malformed message:"
                " (bytes: %d)\n", reply->len);
        return -1;
    }

    // Check the size of the message. We may not have gotten it all.
    while (header.msg_size > reply->len) {
        int old = reply->len;
        while (header.msg_size > reply->max) {
            SOS_buffer_grow(reply, 1 + (header.msg_size - reply->max),
                    SOS_WHOAMI);
        }
        int rest = recv(server_socket_fd, (void *) (reply->data + old),
                header.msg_size - old, 0);
        if (rest < 0) {
            fprintf(stderr, "SOS: recv() call for reply from"
                    " daemon returned an error:\n\t\"(%s)\"\n",
                    strerror(errno));
            break;
        } else {
            dlog(6, "  ... recv() returned %d more bytes.\n", rest);
        }
        reply->len += rest;
    }

    dlog(6, "Reply fully received.  reply->len == %d\n", reply->len);
    return reply->len;

}


int
SOS_target_init(
        SOS_runtime       *sos_context,
        SOS_socket_out  **target,
        char              *target_host,
        int                target_port)
{
    SOS_SET_CONTEXT(sos_context, "SOS_target_init");

    *target = calloc(1, sizeof(SOS_socket_out));
    SOS_socket_out *tgt = *target;
    tgt->sos_context = sos_context;

    tgt->send_lock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_lock(tgt->send_lock);

    if (target_host != NULL) {
        strncpy(tgt->server_host, target_host, NI_MAXHOST);
    } else {
        dlog(0, "WARNING: No host specified during a SOS_target_init."
                "  Defaulting to 'localhost'.\n");
        strncpy(tgt->server_host, SOS_DEFAULT_SERVER_HOST, NI_MAXHOST);
    }

    snprintf(tgt->server_port, NI_MAXSERV, "%d", target_port);

    tgt->buffer_len                = SOS_DEFAULT_BUFFER_MAX;
    tgt->timeout                   = SOS_DEFAULT_MSG_TIMEOUT;
    tgt->server_hint.ai_family     = AF_UNSPEC;     // Allow IPv4 or IPv6
    tgt->server_hint.ai_socktype   = SOCK_STREAM;   // _STREAM/_DGRAM/_RAW
    tgt->server_hint.ai_flags      = AI_NUMERICSERV;// Don't invoke namserv.
    tgt->server_hint.ai_protocol   = 0;             // Any protocol
    //tgt->server_hint.ai_canonname  = NULL;
    //tgt->server_hint.ai_addr       = NULL;
    //tgt->server_hint.ai_next       = NULL;

    pthread_mutex_unlock(tgt->send_lock);

    return 0;
}

int
SOS_target_destroy(SOS_socket_out *target) {
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_destroy");

    pthread_mutex_lock(target->send_lock);
    pthread_mutex_destroy(target->send_lock);

    free(target->send_lock);
    free(target);

    return 0;
}

int
SOS_target_connect(SOS_socket_out *target) {
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_connect");

    int retval = 0;
    int new_fd = 0;

    dlog(8, "Obtaining target send_lock...\n");
    pthread_mutex_lock(target->send_lock);

    dlog(8, "Attempting to open server socket...\n");
    dlog(8, "   ...gathering address info.\n");
    target->server_socket_fd = -1;
    retval = getaddrinfo(target->server_host, target->server_port,
        &target->server_hint, &target->result_list);
    if (retval < 0) {
        dlog(0, "ERROR!  Could not connect to target.  (%s:%s)\n",
            target->server_host, target->server_port );
        pthread_mutex_unlock(target->send_lock);
        return -1;
    }
    
    dlog(8, "   ...iterating possible connection techniques.\n");
    // Iterate the possible connections and register with the SOS daemon:
    for (target->server_addr = target->result_list ;
        target->server_addr != NULL ;
        target->server_addr = target->server_addr->ai_next)
    {
        new_fd = socket(target->server_addr->ai_family,
            target->server_addr->ai_socktype,
            target->server_addr->ai_protocol);
        if (new_fd == -1) { continue; }
        
        retval = connect(new_fd, target->server_addr->ai_addr,
            target->server_addr->ai_addrlen);
        if (retval != -1) break;
        
        close(new_fd);
        new_fd = -1;
    }
    
 
    dlog(8, "   ...freeing unused results.\n");
    freeaddrinfo( target->result_list );
    
    if (new_fd <= 0) {
        dlog(0, "Error attempting to connect to the server.  (%s:%s)\n",
            target->server_host, target->server_port);
        pthread_mutex_unlock(target->send_lock);
        return -1;
    }

    target->server_socket_fd = new_fd;
    dlog(8, "   ...successfully connected!"
            "  target->server_socket_fd == %d\n", target->server_socket_fd);

    return 0;
}


int SOS_target_disconnect(SOS_socket_out *target) {
    SOS_SET_CONTEXT(target->sos_context, "SOS_target_disconnect");
    
    dlog(8, "Closing target file descriptor... (%d)\n",
            target->server_socket_fd);

    close(target->server_socket_fd);
    target->server_socket_fd = -1; 
    dlog(8, "Releasing target send_lock...\n");
    pthread_mutex_unlock(target->send_lock);
    
    dlog(8, "Done.\n");
    return 0;
}

int
SOS_target_send_msg(
        SOS_socket_out *target,
        SOS_buffer *msg)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOS_target_send_msg");

    SOS_msg_header header;
    int            offset      = 0;
    int            inset       = 0;
    int            retval      = 0;
    double         time_start  = 0.0;
    double         time_out    = 0.0;

    if (SOS->status == SOS_STATUS_SHUTDOWN) {
        dlog(1, "Suppressing a send.  (SOS_STATUS_SHUTDOWN)\n");
        return -1;
    }

    if (SOS->config.offline_test_mode == true) {
        dlog(1, "Suppressing a send.  (OFFLINE_TEST_MODE)\n");
        return -1;
    }

    dlog(6, "Processing a send.\n");

    int more_to_send      = 1;
    int failed_send_count = 0;
    int total_bytes_sent  = 0;
    retval = 0;

    SOS_TIME(time_start);
    while (more_to_send) {
        if (failed_send_count >= 8) {
            dlog(0, "ERROR: Unable to contact target after 8 attempts.\n");
            more_to_send = 0;
            pthread_mutex_unlock(target->send_lock);
            return -1;
        }
        retval = send(target->server_socket_fd, (msg->data + total_bytes_sent),
                msg->len, 0);
        if (retval < 0) {
            failed_send_count++;
            dlog(0, "ERROR: Could not send message to target."
                    " (%s)\n", strerror(errno));
            dlog(0, "ERROR:    ...retrying %d more times after"
                    " a brief delay.\n", (8 - failed_send_count));
            usleep(100000);
            continue;
        } else {
            total_bytes_sent += retval;
        }
        if (total_bytes_sent >= msg->len) {
            more_to_send = 0;
        }
    }//while

    dlog(6, "Send complete...\n");
   // Done!

    return total_bytes_sent;
}


void SOS_send_to_daemon(SOS_buffer *message, SOS_buffer *reply ) {
    SOS_SET_CONTEXT(message->sos_context, "SOS_send_to_daemon");

    int rc = 0;
    
    SOS_target_connect(&SOS->net);

    if (rc != 0) {
        dlog(0, "ERROR: Failed attempt to connect to target at %s:%s   (%d)\n",
                SOS->net.server_host,
                SOS->net.server_port,
                rc);
        dlog(0, "ERROR: Ignoring transmission request and returning.\n");
        return;
    }

    rc = SOS_target_send_msg(&SOS->net, message);
    if (rc < 0) {
        fprintf(stderr, "ERROR: Unable to send message to the SOS daemon.\n");
        fflush(stderr);
        return;
    }

    SOS_target_recv_msg(&SOS->net, reply);

    SOS_target_disconnect(&SOS->net);

    return;
}



void SOS_finalize(SOS_runtime *sos_context) {
    SOS_SET_CONTEXT(sos_context, "SOS_finalize");
    
    // Any SOS threads will leave their loops next time they wake up.
    dlog(1, "SOS->status = SOS_STATUS_SHUTDOWN\n");
    SOS->status = SOS_STATUS_SHUTDOWN;

    free(SOS->config.node_id);

    if (SOS->role == SOS_ROLE_CLIENT) {
        if (SOS->config.receives != SOS_RECEIVES_NO_FEEDBACK) {
            dlog(1, "  ... Joining threads...\n");
            dlog(1, "      ... sending empty message to unblock feedback listener\n");
            SOS_socket_out *target = NULL;
            SOS_target_init(SOS, &target, "localhost", SOS->config.receives_port);
            SOS_target_connect(target);
            SOS_buffer *msg = NULL;
            SOS_buffer_init_sized_locking(SOS, &msg, 100, false);
            
            int offset = 0;
            SOS_msg_header header;
            header.msg_size = -1;
            header.msg_type = SOS_MSG_TYPE_FEEDBACK;
            header.msg_from = SOS->my_guid;
            header.ref_guid = -1;
            SOS_buffer_pack(msg, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.ref_guid);
            header.msg_size = offset;
            offset = 0;
            SOS_buffer_pack(msg, &offset, "i", header.msg_size);

            SOS_target_send_msg(target, msg);
            SOS_target_disconnect(target);
            SOS_target_destroy(target);
            SOS_buffer_destroy(msg);
            /*
            dlog(1, "      ... joining feedback thread\n");
            pthread_cond_signal(SOS->task.feedback_cond);
            pthread_cond_destroy(SOS->task.feedback_cond);
            pthread_join(*SOS->task.feedback, NULL);
            pthread_mutex_lock(SOS->task.feedback_lock);
            pthread_mutex_destroy(SOS->task.feedback_lock);
            free(SOS->task.feedback_lock);
            free(SOS->task.feedback_cond);
            free(SOS->task.feedback);
            dlog(1, "      ... done joining threads.\n");
        */
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


void*
SOS_THREAD_receives_direct(void *args)
{
    SOS_SET_CONTEXT((SOS_runtime *) args, "SOS_THREAD_receives_direct");

    SOS->config.receives_ready = -1;
    while (SOS->status == SOS_STATUS_INIT) {
        usleep(10000);
    }
    
    //Get a socket to receive direct feedback messages and begin
    //listening to it.

    SOS_socket_in insock;

    int i;
    int yes;
    int opts;
    int offset;
    SOS_msg_header header;

    yes = 1;

    insock.server_port = "0";    // NOTE: 0 = Request an OS-assigned open port.
    insock.server_socket_fd = -1;
    insock.listen_backlog = 10;

    memset(&insock.server_hint, '\0', sizeof(struct addrinfo));
    insock.server_hint.ai_family     = AF_UNSPEC;     // Allow IPv4 or IPv6
    insock.server_hint.ai_socktype   = SOCK_STREAM;   // _STREAM/_DGRAM/_RAW
    insock.server_hint.ai_flags      = AI_PASSIVE;    // Wildcard IP addresses
    insock.server_hint.ai_protocol   = 0;             // Any protocol
    insock.server_hint.ai_canonname  = NULL;
    insock.server_hint.ai_addr       = NULL;
    insock.server_hint.ai_next       = NULL;
        i = getaddrinfo(NULL, insock.server_port, &insock.server_hint,
        &insock.result);

    if (i != 0) {
        fprintf(stderr, "ERROR: Feedback broken, client-side getaddrinfo()"
            " failed. (%s)\n", gai_strerror(errno));
        fflush(stderr);
        return NULL;
    }

    for (insock.server_addr = insock.result ;
        insock.server_addr != NULL ; 
        insock.server_addr = insock.server_addr->ai_next )
    {
        dlog(1, "Trying an address...\n");

        insock.server_socket_fd = socket(insock.server_addr->ai_family,
            insock.server_addr->ai_socktype, insock.server_addr->ai_protocol);

        if (insock.server_socket_fd < 1) {
            fprintf(stderr, "ERROR: Failed to get a socket.  (%s)\n",
                strerror(errno));
            fflush(stderr);
            continue;
        }

        // Allow this socket to be reused/rebound quickly.
        if (setsockopt(insock.server_socket_fd, SOL_SOCKET, SO_REUSEADDR,
            &yes, sizeof(int)) == -1)
        {
            dlog(0, "  ... could not set socket options.  (%s)\n",
                strerror(errno));
            continue;
        }
       
        insock.server_addr->ai_addrlen = sizeof(struct sockaddr_in);

        if ( bind( insock.server_socket_fd, insock.server_addr->ai_addr,
                insock.server_addr->ai_addrlen ) == -1 ) {
            dlog(0, "  ... failed to bind to socket.  (%s)\n", strerror(errno));
            close( insock.server_socket_fd );
            insock.server_socket_fd = -1;
            continue;
        } 
        // If we get here, we're good to stop looking.
        break;
    }

    if ( insock.server_socket_fd <= 0 ) {
        fprintf(stderr, "ERROR: Client could not socket/setsockopt/"
                "bind to anything to receive feedback. (%d:%s)\n",
                errno, strerror(errno));
         
    } else {
        dlog(0, "  ... got a socket, and bound to it!\n");
    }

    freeaddrinfo(insock.result);

     // Enforce that this is a BLOCKING socket:
    opts = fcntl(insock.server_socket_fd, F_GETFL);
    if (opts < 0) { dlog(0, "ERROR!  Cannot call fcntl() on the"
          " server_socket_fd to get its options.  Carrying on.  (%s)\n",
          strerror(errno));
    }
 
    opts = opts & !(O_NONBLOCK);
    i    = fcntl(insock.server_socket_fd, F_SETFL, opts);
    if (i < 0) { dlog(0, "ERROR!  Cannot use fcntl() to set the"
        " server_socket_fd to BLOCKING mode.  Carrying on.  (%s).\n",
        strerror(errno));
    }

    listen( insock.server_socket_fd, insock.listen_backlog );
    dlog(0, "Listening on socket.\n");
    

    //Part 2: Notify the listener what port we are monitoring.
    // NOTE: Our hostname is in SOS->config.node_id 
    /*   
    char hbuf[NI_MAXHOST] = {0};
    char sbuf[NI_MAXSERV] = {0};
    i = getnameinfo(insock.server_addr->ai_addr, insock.server_addr->ai_addrlen,
            hbuf, sizeof(hbuf), sbuf, sizeof(sbuf),
            NI_NUMERICHOST | NI_NUMERICSERV);  
    printf("host=%s, serv=%s\n", hbuf, sbuf);
    fflush(stdout);
    */
   
    struct sockaddr_in sin;
    socklen_t sin_len = sizeof(sin);
    getsockname(insock.server_socket_fd, (struct sockaddr *)&sin, &sin_len);
    SOS->config.receives_port = ntohs(sin.sin_port);
    SOS->config.receives_ready = 1; 


    //Part 3: Listening loop for feedback messages.
    SOS_buffer *buffer = NULL;
    SOS_buffer_init_sized_locking(SOS, &buffer, SOS_DEFAULT_BUFFER_MAX, false);

    while (SOS->status == SOS_STATUS_RUNNING) {
    
        insock.peer_addr_len = sizeof(struct sockaddr_storage);
        insock.client_socket_fd = accept(insock.server_socket_fd,
                (struct sockaddr *) &insock.peer_addr,
                &insock.peer_addr_len);
        fflush(stdout);
        
        i = getnameinfo((struct sockaddr *) &insock.peer_addr,
                insock.peer_addr_len, insock.client_host,
                NI_MAXHOST, insock.client_port, NI_MAXSERV,
                NI_NUMERICSERV);
        if (i != 0) {
            dlog(0, "Error calling getnameinfo() on client connection."
                    "  (%s)\n", strerror(errno));
            break;
        }

        if (SOS->status == SOS_STATUS_SHUTDOWN) {
            close(insock.client_socket_fd);
            insock.client_socket_fd = -1;
            break;
        }

        offset = 0;
        buffer->len = recv(insock.client_socket_fd, (void *) buffer->data,
                buffer->max, 0);
        dlog(6, "  ... recv() returned %d bytes.\n", buffer->len);

        if (buffer->len < 0) {
            dlog(1, "  ... recv() call returned an errror.  (%s)\n",
                    strerror(errno));
        }

        memset(&header, '\0', sizeof(SOS_msg_header));
        if (buffer->len >= sizeof(SOS_msg_header)) {
            SOS_buffer_unpack(buffer, &offset, "iigg",
                              &header.msg_size,
                              &header.msg_type,
                              &header.msg_from,
                              &header.ref_guid);
        } else {
            dlog(0, "  ... Received short (useless) message.\n");
            continue;
        }

        /* Check the size of the message. We may not have gotten it all. */
        while (header.msg_size > buffer->len) {
            int old = buffer->len;
            while (header.msg_size > buffer->max) {
                SOS_buffer_grow(buffer, 1 + (header.msg_size - buffer->max),
                        SOS_WHOAMI);
            }
            int rest = recv(insock.client_socket_fd,
                    (void *) (buffer->data + old), header.msg_size - old, 0);
            if (rest < 0) {
                dlog(1, "  ... recv() call returned an errror."
                        "  (%s)\n", strerror(errno));
            } else {
                dlog(6, "  ... recv() returned %d more bytes.\n", rest);
            }
            buffer->len += rest;
        }

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


        if (header.msg_type == SOS_FEEDBACK_TYPE_QUERY) {

            if (SOS->config.feedback_handler != NULL) {
                SOS->config.feedback_handler(
                        header.msg_type,
                        header.msg_size,
                        (void *)buffer);
            } else {
                fprintf(stderr, "WARNING: Feedback received but no handler"
                        " has been set. Doing nothing.\n");
            }
            //...

        } else if (header.msg_type == SOS_FEEDBACK_TYPE_PAYLOAD) {

            int            payload_type = header.msg_type;
            int            payload_size = -1;
            void          *payload_data = NULL;

            SOS_buffer_unpack(buffer, &offset, "i", &payload_size);
            payload_data = calloc(payload_size, sizeof(unsigned char));
            memcpy(payload_data, buffer->data + offset, payload_size);
            offset += payload_size;

            /*
             * Uncomment in case of emergency:
             *
            fprintf(stderr, "Message from the server ...\n");
            fprintf(stderr, "   header.msg_size == %d\n", header.msg_size);
            fprintf(stderr, "   header.msg_type == %d\n", header.msg_type);
            fprintf(stderr, "   header.msg_from == %" SOS_GUID_FMT "\n",
                    header.msg_from);
            fprintf(stderr, "   header.ref_guid == %" SOS_GUID_FMT "\n",
                    header.ref_guid);
            fprintf(stderr, "\n");
            fprintf(stderr, "   payload_type == %d\n", payload_type);
            fprintf(stderr, "   payload_size == %d\n", payload_size);
            fprintf(stderr, "   payload_data == \"%s\"\n", (char *) payload_data);
            fprintf(stderr, "   ...\n");
            fprintf(stderr, "\n");
            fflush(stderr);
             *
             */

            if (SOS->config.feedback_handler != NULL) {
                SOS->config.feedback_handler(
                        payload_type,
                        payload_size,
                        payload_data);
            } else {
                fprintf(stderr, "WARNING: Feedback received but no handler"
                        " has been set. Doing nothing.\n");
            }
        }
        close(insock.client_socket_fd);
        insock.client_socket_fd = -1;
    } // while

    SOS_buffer_destroy(buffer);
    dlog(0, "Feedback listener closing down.\n");
    return NULL;
}


void* SOS_THREAD_receives_timed(void *args) {
    SOS_runtime *local_ptr_to_context = (SOS_runtime *) args;
    SOS_SET_CONTEXT(local_ptr_to_context, "SOS_THREAD_receives_timed");
    struct timespec ts;
    struct timeval  tp;
    int wake_type;
    int error_count;

    int offset;
    SOS_msg_header       header;
    SOS_buffer          *check_in_buffer;
    SOS_buffer          *feedback_buffer;
    SOS_feedback_type    feedback;

    if ( SOS->config.offline_test_mode == true ) { return NULL; }

    SOS_buffer_init(SOS, &check_in_buffer);
    SOS_buffer_init(SOS, &feedback_buffer);

    // Wait until the SOS system is up and running or shutting down...
    while ((SOS->status != SOS_STATUS_RUNNING)
            && (SOS->status != SOS_STATUS_SHUTDOWN))
    {
        usleep(1000);
    }

    // Set the wakeup time (ts) to 2 seconds in the future.
    gettimeofday(&tp, NULL);
    ts.tv_sec  = (tp.tv_sec + 2);
    ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;

    // Grab the lock that the wakeup condition is bound to.
    pthread_mutex_lock(SOS->task.feedback_lock);
    error_count = 0;

    while (SOS->status != SOS_STATUS_SHUTDOWN) {
        // Build a checkin message.
        SOS_buffer_wipe(check_in_buffer);
        SOS_buffer_wipe(feedback_buffer);

        header.msg_size = -1;
        header.msg_from = SOS->my_guid;
        header.msg_type = SOS_MSG_TYPE_CHECK_IN;
        header.ref_guid = 0;

        dlog(4, "Building a check-in message.\n");

        offset = 0;
        SOS_buffer_pack(check_in_buffer, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.ref_guid);

        header.msg_size = offset;
        offset = 0;
        SOS_buffer_pack(check_in_buffer, &offset, "i", header.msg_size);

        if (SOS->status != SOS_STATUS_RUNNING) break;

        dlog(4, "Sending check-in to daemon.\n");

        // Ping the daemon to see if there is anything to do.
        SOS_send_to_daemon(check_in_buffer, feedback_buffer);

        dlog(4, "Processing reply (to check-in)...\n");

        memset(&header, '\0', sizeof(SOS_msg_header));
        offset = 0;
        SOS_buffer_unpack(feedback_buffer, &offset, "iigg",
            &header.msg_size,
            &header.msg_type,
            &header.msg_from,
            &header.ref_guid);

        if (header.msg_type != SOS_MSG_TYPE_FEEDBACK) {
            dlog(0, "WARNING: sosd (daemon) responded to a CHECK_IN_MSG"
                " with malformed FEEDBACK!\n");
            error_count++;
            if (error_count > 5) { 
                dlog(0, "ERROR: Too much mal-formed feedback, shutting"
                    " down feedback thread.\n");
                break;
            }
            gettimeofday(&tp, NULL);
            ts.tv_sec  = (tp.tv_sec + 2);
            ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;
            continue;
        } else {
            error_count = 0;
        }

        //TODO: Timed feedback handler
        //...Let the message format settle w/active handler, first
        
        //SOS_process_feedback(feedback_buffer);

        // Set the timer to 2 seconds in the future.
        gettimeofday(&tp, NULL);
        ts.tv_sec  = (tp.tv_sec + 2);
        ts.tv_nsec = (1000 * tp.tv_usec);
        // Go to sleep until the wakeup time (ts) is reached.
        wake_type = pthread_cond_timedwait(SOS->task.feedback_cond,
                SOS->task.feedback_lock, &ts);
        if (wake_type == ETIMEDOUT) {
            // ...Actions on TIMED-OUT (vs. EXPLICIT SIGNAL)
        }
    } //while

    SOS_buffer_destroy(check_in_buffer);
    SOS_buffer_destroy(feedback_buffer);
    pthread_mutex_unlock(SOS->task.feedback_lock);

    return NULL;
}


void SOS_process_feedback(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_process_feedback");
    int               msg_count;
    SOS_msg_header    header;
    int               offset;

    //SLICE: This function is used by ALL THREE types of feedback
    //       modes to process the buffer sent back from the daemon.
    
    //NOTE: The daemon can pack multiple feedback messages into one
    //  reply, in the case that some of them had stacked up between
    //  the checkins in the TIMED/MANUAL modes. The first value in
    //  the buffer is thus an int that says how many messages are
    //  enqueued in the buffer, and (like aggregator messages) we
    //  roll through the buffer for each one extracting out the length
    //  that was encoded in the header, and pass the message on to
    //  the user-supplied callback one at a time.


    dlog(4, "Determining appropriate action RE:feedback from daemon.\n");

    SOS_buffer_lock(buffer);

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "i", &msg_count);

    int msg = 0;
    for (msg = 0; msg < msg_count; msg++) {

        SOS_buffer_unpack(buffer, &offset, "iigg",
                &header.msg_size,
                &header.msg_type,
                &header.msg_from,
                &header.ref_guid);

        //TODO: Process the feedback for all three kinds of handlers.
        //...Waiting on the message format to settle down first.

    }

    SOS_buffer_unlock(buffer);

    return;
}





void
SOS_uid_init(SOS_runtime *sos_context,
        SOS_uid **id_var,
        SOS_guid set_from,
        SOS_guid set_to )
{
    SOS_SET_CONTEXT(sos_context, "SOS_uid_init");
    SOS_uid *id;

    dlog(5, "  ... allocating uid sets\n");
    id = *id_var = (SOS_uid *) malloc(sizeof(SOS_uid));
    id->next = (set_from > 0) ? set_from : 1;
    id->last = (set_to   < SOS_DEFAULT_UID_MAX) ? set_to : SOS_DEFAULT_UID_MAX;
    dlog(5, "     ... default set for uid range"
            " (%" SOS_GUID_FMT " -> %" SOS_GUID_FMT ").\n",
            id->next, id->last);
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


SOS_guid
SOS_uid_next( SOS_uid *id ) {
    SOS_SET_CONTEXT(id->sos_context, "SOS_uid_next");
    long next_serial;
    if (id == NULL) { return -1; }

    dlog(7, "LOCK id->lock\n");
    pthread_mutex_lock( id->lock );

    next_serial = id->next++;

    if (id->next > id->last) {
    // The assumption here is that we're dealing with a GUID, as the other
    // 'local' uid ranges are so large as to effectively guarantee this case
    // will not occur for them.

        if ((SOS->role == SOS_ROLE_LISTENER)
                || (SOS->role == SOS_ROLE_AGGREGATOR)) {
            // NOTE: There is no recourse if a sosd daemon runs out of GUIDs.
            //       That should *never* happen.
            dlog(0, "ERROR:  This sosd instance has run out of GUIDs!"
                    "  Terminating.\n");
            exit(EXIT_FAILURE);
        } else {
            // Acquire a fresh block of GUIDs from the sosd daemon... 
            //TODO:{MEMORY, THREADS} Race condition when requesting new
            //GUID block during the middle of a receive from the daemon.

             SOS_msg_header header;
            SOS_buffer *buf;
            int offset;

            SOS_buffer_init_sized_locking(SOS, &buf,
                    sizeof(SOS_msg_header), false);
            
            dlog(0, "The last guid has been used from SOS->uid.my_guid_pool!"
                    "  Requesting a new block...\n");
            header.msg_size = -1;
            header.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
            header.msg_from = SOS->my_guid;
            header.ref_guid = 0;

            offset = 0;
            SOS_buffer_pack(buf, &offset, "iigg",
                            header.msg_size,
                            header.msg_type,
                            header.msg_from,
                            header.ref_guid);

            header.msg_size = offset;
            offset = 0;
            SOS_buffer_pack(buf, &offset, "i", header.msg_size);

            SOS_buffer *reply;
            SOS_buffer_init_sized_locking(SOS, &reply,
                    SOS_DEFAULT_BUFFER_MAX, false);

            SOS_send_to_daemon(buf, reply);

            offset = 0;
            SOS_buffer_unpack(reply, &offset, "iigg",
                    &header.msg_size,
                    &header.msg_type,
                    &header.msg_from,
                    &header.ref_guid);

            if (SOS->config.offline_test_mode == true) {
                //Do nothing.
            } else {
                // We are a normal client, move us to the next block.
                SOS_buffer_unpack(reply, &offset, "g", &id->next);
                SOS_buffer_unpack(reply, &offset, "g", &id->last);
                dlog(0, "  ... recieved a new guid block from %"
                        SOS_GUID_FMT " to %" SOS_GUID_FMT ".\n",
                        id->next, id->last);
            }
            SOS_buffer_destroy(buf);
            SOS_buffer_destroy(reply);
        }
    }

    dlog(7, "UNLOCK id->lock\n");
    pthread_mutex_unlock( id->lock );

    return next_serial;
}


void
SOS_pub_create(SOS_runtime *sos_context,
    SOS_pub **pub_handle, char *title, SOS_nature nature)
{
     SOS_pub_create_sized(sos_context, pub_handle, title,
             nature, SOS_DEFAULT_ELEM_MAX);
     return;
}

void
SOS_pub_create_sized(SOS_runtime *sos_context,
    SOS_pub **pub_handle, char *title, SOS_nature nature, int new_size)
{
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

    if (SOS->role == SOS_ROLE_AGGREGATOR) {
        new_pub->guid = -1;
    } else {
        new_pub->guid = SOS_uid_next( SOS->uid.my_guid_pool );
    }

    snprintf(new_pub->guid_str, SOS_DEFAULT_STRING_LEN,
            "%" SOS_GUID_FMT, new_pub->guid);

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

    // Set some defaults for the SOS_ROLE_CLIENT's 
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
        dlog(6, "  ... configuring pub to use sos_context->task.val_intake"
                " for snap queue.\n");
        SOS_pipe_init((void *) SOS, &new_pub->snap_queue,
                sizeof(SOS_val_snap *));
    }

    dlog(6, "  ... initializing the name table for values.\n");
    new_pub->name_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(6, "  ... done.\n");
    pthread_mutex_unlock(new_pub->lock);

    *pub_handle = new_pub;

    return;
}




void SOS_expand_data( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_expand_data");

    // NOTE: This is an internal-use-only function, and assumes you
    //       already hold the lock on the pub.
    
    dlog(6, "Growing pub(\"%s\")->elem_max from %d to %d...\n",
            pub->title,
            pub->elem_max,
            (pub->elem_max + SOS_DEFAULT_ELEM_MAX));

    int n = 0;
    int from_old_max = pub->elem_max;
    int to_new_max   = pub->elem_max + SOS_DEFAULT_ELEM_MAX;

    pub->data = (SOS_data **) realloc(pub->data,
            (to_new_max * sizeof(SOS_data *)));

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


int SOS_pack(
        SOS_pub *pub,
        const char *name,
        SOS_val_type pack_type,
        void *pack_val_var)
{
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
    case SOS_VAL_TYPE_BYTES:
        fprintf(stderr, "WARNING: SOS_pack(...) used to pack SOS_VAL_TYPE_BYTES."
                " This is unsupported.\n");
        fprintf(stderr, "WARNING: Please use SOS_pack_bytes(...) instead!\n");
        fprintf(stderr, "WARNING: Doing nothing and returning....\n");
        fflush(stderr);
        return -1;
        break;

    default:
        dlog(0, "ERROR: Invalid pack_type sent to SOS_pack."
                " (%d)\n", (int) pack_type);
        return -1;
        break;
    }


    pthread_mutex_lock(pub->lock);

    // NOTE: Regarding indexing...
    // The hash table will return NULL if a value is not present.
    // The pub->data[elem] index is zero-indexed, so indices are stored +1, to
    //   differentiate between empty and the first position.  The value
    //   returned by SOS_pub_search() is the actual array index to be used.
    
    pos = SOS_pub_search(pub, name);

    if (pos < 0) {
        // Value does NOT EXIST in the pub. 
        // Check if we need to expand the pub
        if (pub->elem_count >= pub->elem_max) {
            SOS_expand_data(pub);
        }

        // Force a pub announce.
        pub->announced = 0;

        // Insert the value... 
        pos = pub->elem_count;
        pub->elem_count++;

        // REMINDER: (pos + 1) is correct.
        // We're storing it's "N'th element" position
        // rather than it's array index.
        // See SOS_pub_search(...) for details.
        pub->name_table->put(pub->name_table, name,
                (void *) ((long)(pos + 1)));

        data = pub->data[pos];

        data->type  = pack_type;
        data->guid  = SOS_uid_next(SOS->uid.my_guid_pool);
        data->val.c_val = NULL;
        data->val_len = 0;
        strncpy(data->name, name, SOS_DEFAULT_STRING_LEN);

    } else {
        // Name ALREADY EXISTS in the pub...
        data = pub->data[pos];
    }

    // Update the value in the pub->data[elem] position.
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
        //Should not be able to get here...
        fprintf(stderr, "ERROR: SOS_VAL_TYPE_BYTES was used in SOS_pack(...)."
                " This is unsupported.\n"
                "ERROR: Please use SOS_pack_bytes(...) instead.\n"
                "ERROR: Somehow libsos passed beyond its safety"
                " guard and has\n"
                "ERROR: modified the state of the publication handle.\n"
                "ERROR: Since safety/consistency can no longer be guaranteed,\n"
                "ERROR: SOS will now terminate. Please update your code.\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
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

    // Place the packed value into the snap_queue...
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
        //Should not be able to get here...
        fprintf(stderr, "ERROR: SOS_VAL_TYPE_BYTES was used in SOS_pack(...)."
                " This is unsupported.\n"
                "ERROR: Please use SOS_pack_bytes(...) instead.\n"
                "ERROR: Somehow libsos passed beyond its safety"
                " guard and has\n"
                "ERROR: modified the state of the publication handle.\n"
                "ERROR: Since safety/consistency can no longer be guaranteed,\n"
                "ERROR: SOS will now terminate. Please update your code.\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
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

    // Done. 
    pthread_mutex_unlock(pub->lock);

    return pos;
}


int
SOS_pack_bytes(
        SOS_pub *pub,
        const char *name,
        int byte_count,
        void *pack_source)
{

    int pos = -1;

    return pos;
}





int
SOS_pub_search(SOS_pub *pub, const char *name)
{
    long i;
    
    i = (long) pub->name_table->get(pub->name_table, name);

    // NOTE: Regarding indexing...
    // If the name does not exist, i==0... since the data elements are
    // zero-indexed, we subtract 1 from i so that 'does not exist' is
    // returned as -1. This is accounted for when values are initially
    // packed, as the name is added to the name_table with the index being
    // (i + 1)'ed.
    
    i--;

    return i;
}


void
SOS_pub_destroy(SOS_pub *pub)
{
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pub_destroy");
    int elem;

    if (SOS->config.offline_test_mode != true) {
        // TODO: { PUB DESTROY } Right now this only works in offline test mode
        // within the client-side library code. The Daemon likely has additional
        // memory structures in play.
        //  ...is this still the case?  */
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



void
SOS_display_pub(SOS_pub *pub, FILE *output_to)
{
    SOS_SET_CONTEXT(pub->sos_context, "SOS_display_pub");
    
    // TODO:{ DISPLAY_PUB }
    // Restore to a the useful CSV/JSON dump that it originally was.
    
    return;
}



void
SOS_val_snap_queue_to_buffer(
        SOS_pub *pub,
        SOS_buffer *buffer,
        bool destroy_snaps)
{
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
    header.ref_guid = pub->guid;

    offset = 0;
    SOS_buffer_pack(buffer, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.ref_guid);

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

        case SOS_VAL_TYPE_INT:
            SOS_buffer_pack(buffer, &offset, "i", snap->val.i_val);
            break;

        case SOS_VAL_TYPE_LONG:
            SOS_buffer_pack(buffer, &offset, "l", snap->val.l_val);
            break;

        case SOS_VAL_TYPE_DOUBLE:
            SOS_buffer_pack(buffer, &offset, "d", snap->val.d_val);
            break;

        case SOS_VAL_TYPE_STRING:
            SOS_buffer_pack(buffer, &offset, "s", snap->val.c_val);
            break;

        case SOS_VAL_TYPE_BYTES:
            SOS_buffer_pack_bytes(buffer, &offset,
                    snap->val_len, snap->val.bytes);

        default:
            dlog(0, "ERROR: Invalid type (%d) at index %d of"
                    " pub->guid == %" SOS_GUID_FMT ".\n",
                    pub->data[snap->elem]->type,
                    snap->elem,
                    pub->guid);
            break;
        }//switch

        if (destroy_snaps == true) {
            if (pub->data[snap->elem]->type == SOS_VAL_TYPE_STRING) {
                free(snap->val.c_val);
            } else if (pub->data[snap->elem]->type == SOS_VAL_TYPE_BYTES) {
                free(snap->val.bytes);
            }
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


void
SOS_val_snap_queue_from_buffer(
        SOS_buffer *buffer,
        SOS_pipe *snap_queue,
        SOS_pub *pub)
{
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_val_snap_queue_from_buffer");
    SOS_msg_header header;
    char           unpack_fmt[SOS_DEFAULT_STRING_LEN] = {0};
    int            offset;
    int            string_len;

    if (pub == NULL) {
        dlog(0, "WARNING! Attempting to build snap_queue for a"
                " pub we don't know about.\n");
        dlog(0, "  ... skipping this request, potentially missing"
                " values in the daemon/database.\n");
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
                      &header.ref_guid);

    dlog(6, "     ... header.msg_size == %d\n", header.msg_size);
    dlog(6, "     ... header.msg_type == %d\n", header.msg_type);
    dlog(6, "     ... header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(6, "     ... header.ref_guid == %" SOS_GUID_FMT "\n", header.ref_guid);

    int snap_index = 0;
    int snap_count = 0;
    SOS_buffer_unpack(buffer, &offset, "i", &snap_count);

    if (snap_count < 1) {
      dlog(1, "WARNING: Attempted to process buffer with ZERO val_snaps."
              " This is unusual.\n");
      dlog(1, "WARNING:    ... since there is no work to do, returning.\n");
      return;
    }

    SOS_val_snap *snap;
    SOS_val_snap **snap_list;
    snap_list = (SOS_val_snap **) calloc(snap_count, sizeof(SOS_val_snap *));

    for (snap_index = 0; snap_index < snap_count; snap_index++) {
        snap_list[snap_index]
                = (SOS_val_snap *) calloc(1, sizeof(SOS_val_snap));
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

        dlog(6, "    ... grabbing element[%d] @ %d/%d(%d) -> type"
                " == %d, val_len == %d\n",
                snap->elem,
                offset,
                header.msg_size,
                buffer->len,
                snap->type,
                snap->val_len);

        switch (snap->type) {

        case SOS_VAL_TYPE_INT:
            SOS_buffer_unpack(buffer, &offset, "i", &snap->val.i_val);
            break;

        case SOS_VAL_TYPE_LONG:
            SOS_buffer_unpack(buffer, &offset, "l", &snap->val.l_val);
            break;

        case SOS_VAL_TYPE_DOUBLE:
            SOS_buffer_unpack(buffer, &offset, "d", &snap->val.d_val);
            break;

        case SOS_VAL_TYPE_STRING:
            snap->val.c_val = (char *) calloc(sizeof(char), snap->val_len);
            SOS_buffer_unpack(buffer, &offset, "s", snap->val.c_val);
            break;

        case SOS_VAL_TYPE_BYTES:
            memset(unpack_fmt, '\0', SOS_DEFAULT_STRING_LEN);
            int rewind_amt = 0;
            int byte_count = 0;
            rewind_amt = SOS_buffer_unpack(buffer, &offset, "i", &byte_count);
            offset -= rewind_amt;
            snap->val_len = byte_count;
            snap->val.bytes = (unsigned char *)
                    calloc(sizeof(unsigned char), (byte_count + 1));
            SOS_buffer_unpack(buffer, &offset, "b", snap->val.bytes);
            break;
        default:
            dlog(6, "ERROR: Invalid type (%d) at index %d with"
                  " pub->guid == %" SOS_GUID_FMT ".\n",
                  snap->type,
                  snap->elem,
                  pub->guid);
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



void
SOS_announce_to_buffer(SOS_pub *pub, SOS_buffer *buffer) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce_to_buffer");
    SOS_msg_header header;
    int   offset;
    int   elem;

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ANNOUNCE;
    header.msg_from = SOS->my_guid;
    header.ref_guid = pub->guid;

    offset = 0;
    SOS_buffer_pack(buffer, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.ref_guid);

    pthread_mutex_lock(pub->lock);

    // Pub metadata.
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

    // Data definitions.
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

    // TODO: { PUB } Come up with better ENUM for announce status. 
    pub->announced = 1;

    pthread_mutex_unlock(pub->lock);

    // Re-pack the message size now that we know it.
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

    // TODO: { ASYNC CLIENT, TIME }
    //       If client-side gets async send, this wont be true.
    //       Until then, buffer-create time IS the send time.
    
    SOS_TIME( send_time );

    if (SOS->role == SOS_ROLE_CLIENT) {
        // Only CLIENT updates the frame when sending, in case this is re-used
        // internally / on the backplane by the LISTENER / AGGREGATOR. */
        this_frame = pub->frame++;
    }

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_PUBLISH;
    header.msg_from = SOS->my_guid;
    header.ref_guid = pub->guid;

    offset = 0;
    SOS_buffer_pack(buffer, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.ref_guid);

    // Pack in the frame of these elements:
    SOS_buffer_pack(buffer, &offset, "l", this_frame);

    // Pack in the data elements.
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
        case SOS_VAL_TYPE_INT:
            SOS_buffer_pack(buffer, &offset,
                    "i", pub->data[elem]->val.i_val);
            break;

        case SOS_VAL_TYPE_LONG:
            SOS_buffer_pack(buffer, &offset,
                    "l", pub->data[elem]->val.l_val);
            break;

        case SOS_VAL_TYPE_DOUBLE:
            SOS_buffer_pack(buffer, &offset,
                    "d", pub->data[elem]->val.d_val);
            break;

        case SOS_VAL_TYPE_STRING:
            SOS_buffer_pack(buffer, &offset,
                    "s", pub->data[elem]->val.c_val);
            dlog(8, "[STRING]: Packing in -> \"%s\" ...\n",
                    pub->data[elem]->val.c_val);
            break;

        case SOS_VAL_TYPE_BYTES:
            dlog(0, "WARNING: Use of SOS_pack(...) to pack bytes"
                    " is discouraged.\n");
            dlog(0, "WARNING: Please use SOS_pack_bytes(...) instead.\n");
            SOS_buffer_pack_bytes(buffer, &offset,
                pub->data[elem]->val_len,
                (void *) pub->data[elem]->val.bytes);
            break;

        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid"
                    " == %" SOS_GUID_FMT ".\n", pub->data[elem]->type,
                    elem, pub->guid);
            break;
        }//switch
    }//for

    // Re-pack the message size now that we know what it is.
    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(buffer, &offset, "i", header.msg_size);

    pthread_mutex_unlock(pub->lock);

    return;
}


void SOS_announce_from_buffer(SOS_buffer *buffer, SOS_pub *pub) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce_from_buffer");
    SOS_msg_header header;
    SOS_data       upd_elem;
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
                      &header.ref_guid);

    pub->guid = header.ref_guid;
    snprintf(pub->guid_str, SOS_DEFAULT_STRING_LEN,
            "%" SOS_GUID_FMT, pub->guid);

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

    // Ensure there is room in this pub to handle incoming data definitions.
    while(pub->elem_max < elem) {
        dlog(6, "  ... growing pub->elem_max from %d to handle"
                " %d elements...\n", pub->elem_max, elem);
        SOS_expand_data(pub);
    }
    pub->elem_count = elem;

    dlog(6, "  ... unpacking the data definitions.\n");
    // Unpack the data definitions:
    elem = 0;
    for (elem = 0; elem < pub->elem_count; elem++) {
        memset(&upd_elem, 0, sizeof(SOS_data));

        SOS_buffer_unpack(buffer, &offset, "gsiiiiiii",
            &upd_elem.guid,
            upd_elem.name,
            &upd_elem.type,
            &upd_elem.meta.freq,
            &upd_elem.meta.semantic,
            &upd_elem.meta.classifier,
            &upd_elem.meta.pattern,
            &upd_elem.meta.compare,
            &upd_elem.meta.mood );

        if ((   pub->data[elem]->guid            != upd_elem.guid )
            || (pub->data[elem]->type            != upd_elem.type )
            || (pub->data[elem]->meta.freq       != upd_elem.meta.freq )
            || (pub->data[elem]->meta.semantic   != upd_elem.meta.semantic )
            || (pub->data[elem]->meta.classifier != upd_elem.meta.classifier )
            || (pub->data[elem]->meta.pattern    != upd_elem.meta.pattern )
            || (pub->data[elem]->meta.compare    != upd_elem.meta.compare )
            || (pub->data[elem]->meta.mood       != upd_elem.meta.mood )) {
        
            // This is a value we have not seen before, or that has
            // changed.  Update the fields and set the sync flag to
            // _RENEW so it gets stored in the database.
            
            pub->data[elem]->sync = SOS_VAL_SYNC_RENEW;

            // Set the element name.
            snprintf(pub->data[elem]->name, SOS_DEFAULT_STRING_LEN,
                    "%s", upd_elem.name);
            // Store the name/position pair in the name_table.
            pub->name_table->put(pub->name_table, pub->data[elem]->name,
                    (void *) ((long)(elem + 1)));

            pub->data[elem]->guid            = upd_elem.guid;
            pub->data[elem]->type            = upd_elem.type;
            pub->data[elem]->meta.freq       = upd_elem.meta.freq;
            pub->data[elem]->meta.semantic   = upd_elem.meta.semantic;
            pub->data[elem]->meta.classifier = upd_elem.meta.classifier;
            pub->data[elem]->meta.pattern    = upd_elem.meta.pattern;
            pub->data[elem]->meta.compare    = upd_elem.meta.compare;
            pub->data[elem]->meta.mood       = upd_elem.meta.mood;

            dlog(8, "  ... pub->data[%d]->guid = %" SOS_GUID_FMT "\n",
                    elem, pub->data[elem]->guid);
            dlog(8, "  ... pub->data[%d]->name = %s\n",
                    elem, pub->data[elem]->name);
            dlog(8, "  ... pub->data[%d]->type = %d\n",
                    elem, pub->data[elem]->type);
            
        } else {
            // This pub is being re-announcd, but we already have
            // identical data for this value.  Don't replace it or
            // change its sync status, as already exists in the db.
            
            continue;
        }

    }//for

    pthread_mutex_unlock(pub->lock);
    dlog(6, "  ... done.\n");

    return;
}

void
SOS_publish_from_buffer(
        SOS_buffer *buffer,
        SOS_pub *pub,
        SOS_pipe *snap_queue)
{
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish_from_buffer");
    SOS_msg_header  header;
    SOS_data       *data;
    long            this_frame;
    int             offset;
    int             elem;

    if (buffer == NULL) {
        dlog(0, "ERROR: SOS_buffer *buffer parameter is NULL!"
                " Terminating.\n");
        exit(EXIT_FAILURE);
    }

    if (pub == NULL) {
        dlog(0, "ERROR: SOS_pub *pub parameter is NULL!"
                " Terminating.\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(pub->lock);

    dlog(7, "Unpacking the values from the buffer...\n");

    offset = 0;
    SOS_buffer_unpack(buffer, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.ref_guid);

    dlog(7, "  ... header.msg_size = %d\n", header.msg_size);
    dlog(7, "  ... header.msg_type = %d\n", header.msg_type);
    dlog(7, "  ... header.msg_from = %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(7, "  ... header.ref_guid = %" SOS_GUID_FMT "\n", header.ref_guid);
    dlog(7, "  ... values:\n");

    SOS_buffer_unpack(buffer, &offset, "l", &this_frame);
    pub->frame = this_frame;

    // Unpack in the data elements.
    while (offset < header.msg_size) {
        dlog(7, "Unpacking next message @ offset %d of %d...\n",
                offset, header.msg_size);

        SOS_buffer_unpack(buffer, &offset, "i", &elem);
        data = pub->data[elem];

        SOS_buffer_unpack(buffer, &offset, "ddill",
                          &data->time.pack,
                          &data->time.send,
                          &data->val_len,
                          &data->meta.semantic,
                          &data->meta.mood);

        dlog(7, "pub->data[%d]->time.pack == %lf"
                "   pub->data[%d]->time.send == %lf\n",
             elem, data->time.pack,
             elem, data->time.send);

        switch (data->type) {

        case SOS_VAL_TYPE_INT:
            SOS_buffer_unpack(buffer, &offset, "i", &data->val.i_val);
            break;

        case SOS_VAL_TYPE_LONG:
            SOS_buffer_unpack(buffer, &offset, "l", &data->val.l_val);
            break;

        case SOS_VAL_TYPE_DOUBLE:
            SOS_buffer_unpack(buffer, &offset, "d", &data->val.d_val);
            break;
        
        case SOS_VAL_TYPE_STRING:
            if (data->val.c_val != NULL) {
                free(data->val.c_val );
            }
            data->val.c_val = (char *) malloc(1 + data->val_len);
            memset(data->val.c_val, '\0', (1 + data->val_len));
            SOS_buffer_unpack(buffer, &offset, "s", data->val.c_val);
            dlog(8, "[STRING] Extracted pub message string: %s\n",
                    data->val.c_val);
            break;

        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid"
                    " == %" SOS_GUID_FMT ".\n", data->type, elem, pub->guid);
            break;
        }

        data->state = SOS_VAL_STATE_CLEAN;

    }//while

    pthread_mutex_unlock(pub->lock);
    dlog(7, "  ... done.\n");

    return;
}


void
SOS_announce( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce");
    SOS_buffer *ann_buf;
    SOS_buffer *rep_buf;

    SOS_buffer_init(SOS, &ann_buf);
    SOS_buffer_init_sized(SOS, &rep_buf, SOS_DEFAULT_REPLY_LEN);

    dlog(6, "Preparing an announcement message...\n");

    if (pub->announced != 0) {
        dlog(0, "WARNING: This publication has already been announced!"
                " Doing nothing.\n");
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


void
SOS_publish( SOS_pub *pub ) {
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


