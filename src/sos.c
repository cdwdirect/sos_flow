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

#ifdef USE_MUNGE
#include <munge.h>
#endif

#include "sos.h"
#include "sos_types.h"
#include "sos_options.h"
#include "sos_debug.h"
#include "sos_buffer.h"
#include "sos_pipe.h"
#include "sos_qhashtbl.h"
#include "sos_target.h"

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
void         SOS_process_feedback(SOS_buffer *buffer);

/**
 * @brief An internal utility function for growing a pub to hold more data.
 */
void         SOS_expand_data(SOS_pub *pub);

/**
 * @brief Launch a thread for feedback handling specified by the user.
 */
void SOS_receiver_init(SOS_runtime *sos_context);

char global_placeholder_RETURN_FAIL;
char global_placeholder_RETURN_BUSY;

/* Helper function to diagnose deadlocks. */
/* set this define to enable the checks: */
// #define DEBUG_DEADLOCK

#ifdef DEBUG_DEADLOCK
static unsigned int pub_lock_count = 0;
#endif

static inline void _sos_lock_pub(SOS_pub * pub, const char * func) {
#ifdef DEBUG_DEADLOCK
    fprintf(stderr, "Locking in %s.\n", func);
#endif
    pthread_mutex_lock(pub->lock);
#ifdef DEBUG_DEADLOCK
    pub_lock_count++;
    if (pub_lock_count > 1) {
        fprintf(stderr, "ERROR! Locked pub multiple times.\n"); 
        fflush(stderr);
        abort();
    }
#endif
}
static inline void _sos_unlock_pub(SOS_pub * pub, const char * func) {
#ifdef DEBUG_DEADLOCK
    fprintf(stderr, "Unlocking in %s.\n", func); 
    pub_lock_count--;
    if (pub_lock_count < 0) {
        fprintf(stderr, "ERROR! Unlocked pub multiple times.\n"); 
        fflush(stderr);
        abort();
    }
#endif
    pthread_mutex_unlock(pub->lock);
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
 * @param sos_runtime The address of an uninitialized SOS_runtime pointer.
 * @param role What this client is doing, e.g: @c SOS_ROLE_CLIENT
 * @param receives What feedback is expected, e.g: @c SOS_RECEIVES_NO_FEEDBACK
 * @param handler Function pointer to a user-defined feedback handler.
 * @warning The SOS daemon needs to be up and running before calling.
 */
void
SOS_init(SOS_runtime **sos_runtime,
    SOS_role role, SOS_receives receives, SOS_feedback_handler_f handler)
{
    *sos_runtime = (SOS_runtime *) malloc(sizeof(SOS_runtime));
     memset(*sos_runtime, '\0', sizeof(SOS_runtime));

    // If these are needed (as in daemons) then call
    // SOS_init_existing_runtime() after setting things
    // up yourself, as in sosd.c:
    (*sos_runtime)->config.argc = -1;
    (*sos_runtime)->config.argv = NULL;
    (*sos_runtime)->config.options_file  = getenv("SOS_OPTIONS_FILE");
    (*sos_runtime)->config.options_class = getenv("SOS_OPTIONS_CLASS");

    SOS_init_existing_runtime(sos_runtime, role, receives, handler);
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
SOS_init_existing_runtime(
        SOS_runtime **sos_runtime,
        SOS_role role,
        SOS_receives receives,
        SOS_feedback_handler_f handler)
{
    SOS_msg_header header;
    int i, n, retval, remote_socket_fd;
    int rc;
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

    SOS_options *opt = NULL;
    
    SOS_options_init(NEW_SOS, &opt,
            NEW_SOS->config.options_file,
            NEW_SOS->config.options_class);

    NEW_SOS->config.options = opt;

    NEW_SOS->status = SOS_STATUS_INIT;
    NEW_SOS->config.layer = SOS_LAYER_DEFAULT;
    NEW_SOS->config.receives = receives;
    NEW_SOS->config.feedback_handler = handler;
    NEW_SOS->config.receives_port = -1;
    NEW_SOS->config.receives_ready = -1;
    NEW_SOS->config.process_id = (int) getpid();

    NEW_SOS->config.program_name = (char *) calloc(PATH_MAX, sizeof(char));
    rc = readlink("/proc/self/exe", NEW_SOS->config.program_name, PATH_MAX);
    if (rc < 0) { 
        fprintf (stderr, "ERROR: Unable to read /proc/self/exe\n");
        exit (EXIT_FAILURE);
    }

    // The SOS_SET_CONTEXT macro makes a new variable, 'SOS'...
    SOS_SET_CONTEXT(NEW_SOS, "SOS_init");

    dlog(1, "Initializing SOS ...\n");
    dlog(4, "  ... importing options into SOS->config.\n");

#ifdef USE_MUNGE
    //Optionally grab a Munge credential:
    munge_ctx_t munge_ctx;
    munge_err_t munge_err;
    if (!(munge_ctx = munge_ctx_create ())) {
        fprintf (stderr, "ERROR: Unable to create MUNGE context\n");
        exit (EXIT_FAILURE);
    }
    char munge_socket[] = "/var/run/munge/moab.socket.2";
    munge_err = munge_ctx_set (munge_ctx, MUNGE_OPT_SOCKET, &munge_socket);
    if (munge_err != EMUNGE_SUCCESS) {
        fprintf (stderr, "ERROR: %s\n", munge_ctx_strerror (munge_ctx));
        exit (EXIT_FAILURE);
    }
    SOS->my_cred = NULL;
    dlog(1, "Obtaining MUNGE credential...\n");
    munge_err = munge_encode(&SOS->my_cred, munge_ctx, NULL, 0);
    if (munge_err != EMUNGE_SUCCESS) {
        fprintf (stderr, "ERROR: %s\n", munge_ctx_strerror (munge_ctx));
        exit (EXIT_FAILURE);
    }
    dlog(1, "   credential == \"%s\"\n", SOS->my_cred);
#endif

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

        SOS->daemon = NULL;
        const char *portStr = getenv("SOS_CMD_PORT");
        if (portStr == NULL) {
            portStr = SOS_DEFAULT_SERVER_PORT;
        }
        SOS_target_init(SOS, &SOS->daemon, SOS_DEFAULT_SERVER_HOST,
                atoi(portStr));
        rc = SOS_target_connect(SOS->daemon);

        if (rc != 0) {
            //fprintf(stderr, "Unable to connect to an SOSflow daemon on"
            //        " port %d.  (rc == %d)\n",
            //        atoi(getenv("SOS_CMD_PORT")), rc);
            free(SOS->config.node_id);
            free(SOS->config.program_name);
            free(SOS);
            *sos_runtime = NULL;
            return;
        }

        dlog(4, "  ... registering this instance with SOS->   (%s:%s)\n",
        SOS->daemon->remote_host, SOS->daemon->remote_port);

        SOS_buffer *buffer = NULL;
        SOS_buffer_init_sized(SOS, &buffer, 1024);

        header.msg_size = -1;
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = 0;
        header.ref_guid = 0;

        int offset = 0;
        SOS_msg_zip(buffer, header, 0, &offset);

        //Send client version information:
        SOS_buffer_pack(buffer, &offset, "ii",
            SOS_VERSION_MAJOR,
            SOS_VERSION_MINOR);

        int client_uid = getuid();
        SOS_buffer_pack(buffer, &offset, "i",
                client_uid);

        header.msg_size = offset;
        offset = 0;
        SOS_msg_zip(buffer, header, 0, &offset);

        dlog(4, "Built a registration message:\n");
        dlog(4, "  ... buffer->data == %ld\n", (long) buffer->data);
        dlog(4, "  ... buffer->len  == %d\n", buffer->len);
        dlog(4, "Calling send...\n");

        retval = SOS_target_send_msg(SOS->daemon, buffer);

        if (retval < 0) {
            fprintf(stderr, "ERROR: Could not write to server socket!"
                    "  (%s:%s)\n",
            SOS->daemon->remote_host, SOS->daemon->remote_port);
            SOS_target_destroy(SOS->daemon);
            free(SOS->config.node_id);
            free(SOS->config.program_name);
            free(*sos_runtime);
            *sos_runtime = NULL;
            return;
        } else {
            dlog(4, "Registration message sent.   (retval == %d)\n", retval);
        }


        SOS_buffer_wipe(buffer);
        retval = SOS_target_recv_msg(SOS->daemon, buffer);

        if (retval < 1) {
            fprintf(stderr, "ERROR: Daemon does not appear to be running.\n");
            SOS_target_disconnect(SOS->daemon);
            SOS_target_destroy(SOS->daemon);
            free(SOS->config.node_id);
            free(SOS->config.program_name);
            free(*sos_runtime);
            *sos_runtime = NULL;
            return;
        }

        dlog(4, "  ... server responded with %d bytes.\n", retval);

        SOS_target_disconnect(SOS->daemon);

        offset = 0;
        SOS_msg_unzip(buffer, &header, 0, &offset);

        SOS_buffer_unpack(buffer, &offset, "gg",
                &guid_pool_from,
                &guid_pool_to);

        int server_version_major = -1;
        int server_version_minor = -1;

        SOS_buffer_unpack(buffer, &offset, "ii",
                &server_version_major,
                &server_version_minor);

        int server_uid = -1;

        SOS_buffer_unpack(buffer, &offset, "i",
                &server_uid);

        //TODO: Make UID validation a runtime setting.
        //      If it is disabled, emit a non-scary notification anyway.

        if (server_uid != client_uid) {
            fprintf(stderr, "ERROR: SOS daemon's UID (%d) does not"
                    " match yours (%d)!  Connection refused.\n",
                    server_uid, client_uid);
            SOS_target_destroy(SOS->daemon);
            free(SOS->config.node_id);
            free(SOS->config.program_name);
            free(*sos_runtime);
            *sos_runtime = NULL;
            return;
        }

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

        //slice

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

    SOS_TIME(SOS->config.time_of_init);

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

    msg = NULL;
    reply = NULL;
    SOS_buffer_init_sized(SOS, &msg, 1024);
    SOS_buffer_init_sized(SOS, &reply, 256);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SENSITIVITY;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

    offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

    SOS_buffer_pack(msg, &offset, "ssi",
            handle,
            SOS->config.node_id,
            SOS->config.receives_port);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

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

    msg = NULL;
    reply = NULL;
    SOS_buffer_init_sized(SOS, &msg, SOS_DEFAULT_BUFFER_MAX);
    SOS_buffer_init_sized(SOS, &reply, 256);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_TRIGGERPULL;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

    int offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

    SOS_buffer_pack(msg, &offset, "sis",
            handle,
            data_length,
            data);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

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
// length in the correct part of the header, and potentially
// embeds Munge signatures.
int
SOS_msg_zip(
        SOS_buffer *msg,
        SOS_msg_header header,
        int starting_offset,
        int *offset_after_header)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOS_message_zip");

    if (msg->is_locking) {
        pthread_mutex_lock(msg->lock);
    }

    int offset = starting_offset;
    SOS_buffer_pack(msg, &offset, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.ref_guid);

#ifdef USE_MUNGE
    if (SOS->my_cred != NULL) {
        SOS_buffer_pack(msg, &offset, "s", SOS->my_cred);
        msg->ref_cred = strdup(SOS->my_cred);
    } else {
        fprintf(stderr, "CRITICAL WARNING: Attempting to zip a message for"
                " which there is no munge credential!\n");
        fflush(stderr);
    }
#endif

    *offset_after_header = offset;
    msg->is_zipped = true;

    if (msg->is_locking) {
        pthread_mutex_unlock(msg->lock);
    }

    return (offset - starting_offset);
}


int
SOS_msg_unzip(
        SOS_buffer *msg,
        SOS_msg_header *header,
        int starting_offset,
        int *offset_after_header)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOS_msg_unzip");

    if (msg->is_locking) {
        pthread_mutex_unlock(msg->lock);
    }

    int offset = starting_offset;
    SOS_buffer_unpack(msg, &offset, "iigg",
            &header->msg_size,
            &header->msg_type,
            &header->msg_from,
            &header->ref_guid);

#ifdef USE_MUNGE
    header->ref_cred = NULL;
    SOS_buffer_unpack_safestr(msg, &offset, &header->ref_cred);
#endif

    *offset_after_header = offset;
    msg->is_zipped = false;

    if (msg->is_locking) {
        pthread_mutex_unlock(msg->lock);
    }

    return offset;
}


int
SOS_msg_seal(
        SOS_buffer *msg,
        SOS_msg_header header,
        int starting_offset,
        int *offset_after_header_size_field)
{
    SOS_SET_CONTEXT(msg->sos_context, "SOS_message_seal");
    // A small utility function to set the header size field
    // before sending over the wire, to be used when assembling
    // custom message buffers (rarely).

    if (msg->is_locking) {
        pthread_mutex_lock(msg->lock);
    }

    // Note: We do not change the 'is_zipped' flag here, since
    // we're merely adjusting the size field.

    int offset = starting_offset;
    SOS_buffer_pack(msg, &offset, "i", header.msg_size);
    *offset_after_header_size_field = offset;

    if (msg->is_locking) {
        pthread_mutex_unlock(msg->lock);
    }

    return offset;
}



void SOS_send_to_daemon(SOS_buffer *message, SOS_buffer *reply ) {
    SOS_SET_CONTEXT(message->sos_context, "SOS_send_to_daemon");

    int rc = 0;
    rc = SOS_target_connect(SOS->daemon);
    if (rc != 0) {
        dlog(0, "ERROR: Failed attempt to connect to target at %s:%s   (%d)\n",
                SOS->daemon->local_host,
                SOS->daemon->local_port,
                rc);
        dlog(0, "ERROR: Ignoring transmission request and returning.\n");
        return;
    }

    rc = SOS_target_send_msg(SOS->daemon, message);

    if (rc < 1) {
        fprintf(stderr, "ERROR: Unable to send message to the SOS daemon.\n");
        fflush(stderr);
        return;
    }

    SOS_target_recv_msg(SOS->daemon, reply);

    SOS_target_disconnect(SOS->daemon);

    return;
}



void SOS_finalize(SOS_runtime *sos_context) {
    SOS_SET_CONTEXT(sos_context, "SOS_finalize");

    // Any SOS threads will leave their loops next time they wake up.
    dlog(1, "SOS->status = SOS_STATUS_SHUTDOWN\n");
    SOS->status = SOS_STATUS_SHUTDOWN;


    if (SOS->role == SOS_ROLE_CLIENT) {
        dlog(1, "    Closing down client-related items...\n");
        if (SOS->config.receives == SOS_RECEIVES_DIRECT_MESSAGES) {
            dlog(1, "  ... This client RECEIVES_DIRECT_MESSAGES:\n");
            dlog(1, "      ... establishing connection it self...\n");
            SOS_socket *target = NULL;
            SOS_target_init(SOS, &target, SOS_DEFAULT_SERVER_HOST, SOS->config.receives_port);
            dlog(1, "      ... connecting to self...\n");
            SOS_target_connect(target);
            SOS_buffer *msg = NULL;
            SOS_buffer_init_sized_locking(SOS, &msg, 1024, false);

            // Send ourselves an empty feedback message to
            // flush the socket accept() and return from that thread.
            int offset = 0;
            SOS_msg_header header;
            header.msg_size = -1;
            header.msg_type = SOS_MSG_TYPE_FEEDBACK;
            header.msg_from = SOS->my_guid;
            header.ref_guid = -1;
            SOS_msg_zip(msg, header, 0, &offset);

            header.msg_size = offset;
            offset = 0;
            SOS_msg_zip(msg, header, 0, &offset);

            dlog(1, "      ... sending empty feedback message to self...\n");
            SOS_target_send_msg(target, msg);
            dlog(1, "      ... disconnecting and destroying connection/msg...\n");
            SOS_target_disconnect(target);
            SOS_target_destroy(target);
            SOS_buffer_destroy(msg);

            // *  NOTE: The below code is not needed, as the above message
            // *  will flush the thread out, if not a message in the queue
            // *  or incoming from the daemon.
            //dlog(1, "      ... joining feedback thread\n");
            //pthread_cond_signal(SOS->task.feedback_cond);
            //pthread_cond_destroy(SOS->task.feedback_cond);
            //pthread_join(*SOS->task.feedback, NULL);
            //pthread_mutex_lock(SOS->task.feedback_lock);
            //pthread_mutex_destroy(SOS->task.feedback_lock);
            //free(SOS->task.feedback_lock);
            //free(SOS->task.feedback_cond);
            //free(SOS->task.feedback);
            //dlog(1, "      ... done joining threads.\n");
        }

        dlog(1, "  ... Removing send lock...\n");
        if (SOS->daemon != NULL) {
            pthread_mutex_lock(SOS->daemon->send_lock);
            pthread_mutex_destroy(SOS->daemon->send_lock);
            free(SOS->daemon->send_lock);
        }

        dlog(1, "  ... Releasing uid objects...\n");
        SOS_uid_destroy(SOS->uid.local_serial);
        SOS_uid_destroy(SOS->uid.my_guid_pool);

    }
    free(SOS->config.node_id);

    dlog(1, "Done!\n");
    free(SOS);

    return;
}


void*
SOS_THREAD_receives_direct(void *args)
{
    SOS_SET_CONTEXT((SOS_runtime *) args, "SOS_THREAD_receives_direct");

    SOS->config.receives_ready = -1;
    while (SOS->status != SOS_STATUS_RUNNING) {
        usleep(10000);
    }

    dlog(1, "SOS is up and running... entering feedback listen loop.\n");

    SOS_socket *insock;

    int i;
    int yes;
    int opts;
    int offset;
    SOS_msg_header header;
    char local_hostname[NI_MAXHOST];

    insock = NULL;
    SOS_target_init(SOS, &insock, SOS_DEFAULT_SERVER_HOST, 0);
    SOS_target_setup_for_accept(insock);

    //Gather some information about this socket:
    struct sockaddr_in sin;
    socklen_t sin_len = sizeof(sin);
    getsockname(insock->local_socket_fd, (struct sockaddr *)&sin, &sin_len);
    SOS->config.receives_port = ntohs(sin.sin_port);
    SOS->config.receives_ready = 1;

    dlog(1, "SOS->config.receives_port == %d\n",
            SOS->config.receives_port);

    //Part 3: Listening loop for feedback messages.
    SOS_buffer *buffer = NULL;
    SOS_buffer_init_sized_locking(SOS, &buffer, SOS_DEFAULT_BUFFER_MAX, false);

    while (SOS->status == SOS_STATUS_RUNNING) {
        SOS_buffer_wipe(buffer);

        dlog(5, "Waiting for a connection.\n");
        SOS_target_accept_connection(insock);
        dlog(5, "Connection received!\n");

        if (SOS->status == SOS_STATUS_SHUTDOWN) {
            SOS_target_disconnect(insock);
            break;
        }

        i = SOS_target_recv_msg(insock, buffer);
        if (i < sizeof(SOS_msg_header)) {
            SOS_target_disconnect(insock);
            continue;
        };

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


        if (header.msg_type == SOS_FEEDBACK_TYPE_QUERY) {
            dlog(5, "Returning query results to the feedback"
                    " handler function.\n");
            if (SOS->config.feedback_handler != NULL) {
                SOS->config.feedback_handler(
                        header.msg_type,
                        header.msg_size,
                        (void *)buffer);
            } else {
                fprintf(stderr, "WARNING: Feedback (QUERY RESULTS) received"
                        " but no handler has been set. Doing nothing.\n");
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

            //
            // NOTE: Uncomment in case of emergency... useful for debugging.  :)
            //
            //fprintf(stderr, "Message from the server ...\n");
            //fprintf(stderr, "   header.msg_size == %d\n", header.msg_size);
            //fprintf(stderr, "   header.msg_type == %d\n", header.msg_type);
            //fprintf(stderr, "   header.msg_from == %" SOS_GUID_FMT "\n",
            //        header.msg_from);
            //fprintf(stderr, "   header.ref_guid == %" SOS_GUID_FMT "\n",
            //        header.ref_guid);
            //fprintf(stderr, "\n");
            //fprintf(stderr, "   payload_type == %d\n", payload_type);
            //fprintf(stderr, "   payload_size == %d\n", payload_size);
            //fprintf(stderr, "   payload_data == \"%s\"\n", (char *) payload_data);
            //fprintf(stderr, "   ...\n");
            //fprintf(stderr, "\n");
            //fflush(stderr);
            //

            if (SOS->config.feedback_handler != NULL) {
                dlog(5, "Sending payload to the feedback handler.\n");
                SOS->config.feedback_handler(
                        payload_type,
                        payload_size,
                        payload_data);
            } else {
                fprintf(stderr, "WARNING: Feedback (PAYLOAD) received but"
                        " no handler has been set. Doing nothing.\n");
            }
        }
        SOS_target_disconnect(insock);
    } // while

    SOS_buffer_destroy(buffer);
    dlog(1, "Feedback listener closing down.\n");
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

    check_in_buffer = NULL;
    feedback_buffer = NULL;
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

    fprintf(stderr, "CRITICAL WARNING: You have selected TIMED"
            " feedback checkins. This feature is not presently"
            " supported!\n"
            "                 Please use DIRECT or NO feedback.\n");

    while (SOS->status != SOS_STATUS_SHUTDOWN) {
        // Build a checkin message.
        SOS_buffer_wipe(check_in_buffer);
        SOS_buffer_wipe(feedback_buffer);

        dlog(4, "Building a check-in message.\n");

        header.msg_size = -1;
        header.msg_from = SOS->my_guid;
        header.msg_type = SOS_MSG_TYPE_CHECK_IN;
        header.ref_guid = 0;

        offset = 0;
        SOS_msg_zip(check_in_buffer, header, 0, &offset);

        header.msg_size = offset;
        offset = 0;
        SOS_msg_zip(check_in_buffer, header, 0, &offset);

        if (SOS->status != SOS_STATUS_RUNNING) break;

        dlog(4, "Sending check-in to daemon.\n");

        // Ping the daemon to see if there is anything to do.
        SOS_send_to_daemon(check_in_buffer, feedback_buffer);

        dlog(4, "Processing reply (to check-in)...\n");

        memset(&header, '\0', sizeof(SOS_msg_header));
        offset = 0;
        SOS_msg_unzip(feedback_buffer, &header, 0, &offset);

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

        // HERE we would extract the message from the buffer...
        // ...
        //
        // THEN we would unzip it
        // ...

        SOS_msg_unzip(buffer, &header, offset, &offset);


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

    dlog(3, "  ... allocating uid sets\n");
    id = *id_var = (SOS_uid *) malloc(sizeof(SOS_uid));
    id->next = (set_from > 0) ? set_from : 1;
    id->last = (set_to   < SOS_DEFAULT_UID_MAX) ? set_to : SOS_DEFAULT_UID_MAX;
    dlog(3, "     ... default set for uid range"
            " (%" SOS_GUID_FMT " -> %" SOS_GUID_FMT ").\n",
            id->next, id->last);
    dlog(3, "     ... initializing uid mutex.\n");
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

            buf = NULL;
            SOS_buffer_init_sized_locking(SOS, &buf,
                    sizeof(SOS_msg_header), false);

            dlog(1, "The last guid has been used from SOS->uid.my_guid_pool!"
                    "  Requesting a new block...\n");
            header.msg_size = -1;
            header.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
            header.msg_from = SOS->my_guid;
            header.ref_guid = 0;

            offset = 0;
            SOS_msg_zip(buf, header, 0, &offset);

            header.msg_size = offset;
            offset = 0;
            SOS_msg_zip(buf, header, 0, &offset);

            SOS_buffer *reply;

            reply = NULL;
            SOS_buffer_init_sized_locking(SOS, &reply,
                    SOS_DEFAULT_BUFFER_MAX, false);

            SOS_send_to_daemon(buf, reply);

            offset = 0;
            SOS_msg_unzip(reply, &header, 0, &offset);

            if (SOS->config.offline_test_mode == true) {
                //Do nothing.
            } else {
                // We are a normal client, move us to the next block.
                SOS_buffer_unpack(reply, &offset, "g", &id->next);
                SOS_buffer_unpack(reply, &offset, "g", &id->last);
                dlog(1, "  ... recieved a new guid block from %"
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
SOS_pub_init(SOS_runtime *sos_context,
    SOS_pub **pub_handle, const char *title, SOS_nature nature)
{
     SOS_pub_init_sized(sos_context, pub_handle, title,
             nature, SOS_DEFAULT_ELEM_MAX);
     return;
}

void
SOS_pub_init_sized(SOS_runtime *sos_context,
    SOS_pub **pub_handle, const char *title, SOS_nature nature, int new_size)
{
    SOS_SET_CONTEXT(sos_context, "SOS_pub_init_sized");
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
    new_pub->cache_depth      = SOS->config.options->pub_cache_depth;

    dlog(6, "  ... constructing cache ring buffer.\n");
    int cache_alloc_size = 1;
    if (new_pub->cache_depth > 0) {
        cache_alloc_size = new_pub->cache_depth;
    } else {
        cache_alloc_size = 1;
    }
    /* Don't allocate the cache yet - wait until we know the depth
     * from the client */
    new_pub->cache = NULL;
    new_pub->cache_head = 0;

    dlog(6, "  ... zero-ing out the strings.\n");

    strncpy(new_pub->node_id, SOS->config.node_id, SOS_DEFAULT_STRING_LEN);
    new_pub->process_id = SOS->config.process_id;
    strncpy(new_pub->prog_name, SOS->config.program_name, SOS_DEFAULT_STRING_LEN);

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
            //
            new_pub->data[i]->meta.freq        = SOS_VAL_FREQ_DEFAULT;
            new_pub->data[i]->meta.classifier  = SOS_VAL_CLASS_DATA;
            new_pub->data[i]->meta.semantic    = SOS_VAL_SEMANTIC_DEFAULT;
            new_pub->data[i]->meta.pattern     = SOS_VAL_PATTERN_DEFAULT;
            new_pub->data[i]->meta.compare     = SOS_VAL_COMPARE_SELF;
            new_pub->data[i]->meta.relation_id = 0;
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

void SOS_pub_config(SOS_pub *pub, SOS_pub_option opt, ...) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pub_config");

    //NOTE: Each option will have its own expectations
    //      for what is in the va_list.  Determine what
    //      to scrape out inside the switch below.
    va_list  ap;
    va_start(ap, opt);
    //
    int      i;
    double   d;
    char    *c;

    int offset = 0;
    SOS_buffer *msg = NULL;
    SOS_buffer *reply = NULL;
    SOS_msg_header header;

    // Be conservative and grab the lock the whole time any
    // options are being set.  Configuration is a relatively
    // rare event, better safe than sorry -- this will prevent
    // extremely rare cases such as one thread announcing
    // and another adjusting cache depth, with a race
    // condition that causes the notification to not be sent
    // to the daemon.
    pthread_mutex_lock(pub->lock);

    switch (opt) {

    case SOS_PUB_OPTION_CACHE:
        // Get the new pub cache depth: 
        i = va_arg(ap, int);


        if (pub->cache_depth != i) {
            pub->cache_depth = i;

            // NOTE: Only send message IF the pub has been
            //       announced/published already, otherwise
            //       the newly set size will go out with the
            //       first publish.

            if (pub->announced > 0) {
                // NOTE: Create a SOS_MSG_TYPE_CACHE_SIZE message
                //       and send it to the listener.  (It will
                //       forward it to the aggregator as needed.)

                offset = 0;
                SOS_buffer_init_sized_locking(SOS, &msg, 1024, false);
                SOS_buffer_init_sized_locking(SOS, &reply, 1024, false);
                header.msg_size = -1;
                header.msg_type = SOS_MSG_TYPE_CACHE_SIZE;
                header.msg_from = SOS->my_guid;
                header.ref_guid = pub->guid;
                SOS_msg_zip(msg, header, 0, &offset);
                SOS_buffer_pack(msg, &offset, "gi",
                        pub->guid,
                        pub->cache_depth);
                offset = 0;
                SOS_msg_zip(msg, header, 0, &offset);
                SOS_send_to_daemon(msg, reply);
                SOS_buffer_destroy(msg);
                SOS_buffer_destroy(reply);
            }
            // Done adjusting the cache_size and giving notice.

        } else {
            // If we got here it is because the pub is already
            // set to the cache_depth being requested.
            //
            // Do nothing.
        }
        break; //end: SOS_PUB_OPTION_CACHE

    default:
        dlog(1, "WARNING: Invalid option, doing nothing. (%d)\n", opt);
        pthread_mutex_unlock(pub->lock);
        return;
    }

    pthread_mutex_unlock(pub->lock);

    return;
}


void SOS_expand_data( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_expand_data");

    // NOTE: This is an internal-use-only function, and assumes you
    //       already hold the lock on the pub.

    double start_time = 0.0;
    double stop_time  = 0.0;

    dlog(6, "Growing pub(\"%s\")->elem_max from %d to %d...\n",
            pub->title,
            pub->elem_max,
            (pub->elem_max + SOS_DEFAULT_ELEM_MAX));

    SOS_TIME(start_time);

    int n = 0;
    int from_old_max = pub->elem_max;
    int to_new_max   = pub->elem_max + SOS_DEFAULT_ELEM_MAX;

    pub->data = (SOS_data **) realloc(pub->data,
            (to_new_max * sizeof(SOS_data *)));

    for (n = from_old_max; n < to_new_max; n++) {
        pub->data[n] = calloc(1, sizeof(SOS_data));

        pub->data[n]->guid      = 0;
        pub->data[n]->name[0]   = '\0';
        pub->data[n]->type      = SOS_VAL_TYPE_INT;
        pub->data[n]->val_len   = 0;
        pub->data[n]->val.l_val = 0;
        pub->data[n]->val.c_val = 0;
        pub->data[n]->val.d_val = 0.0;
        pub->data[n]->state     = SOS_VAL_STATE_EMPTY;
        pub->data[n]->time.pack = 0.0;
        pub->data[n]->time.send = 0.0;
        pub->data[n]->time.recv = 0.0;
        
        pub->data[n]->meta.freq        = SOS_VAL_FREQ_DEFAULT;
        pub->data[n]->meta.classifier  = SOS_VAL_CLASS_DATA;
        pub->data[n]->meta.semantic    = SOS_VAL_SEMANTIC_DEFAULT;
        pub->data[n]->meta.pattern     = SOS_VAL_PATTERN_DEFAULT;
        pub->data[n]->meta.compare     = SOS_VAL_COMPARE_SELF;
        pub->data[n]->meta.relation_id = 0;
    }

    pub->elem_max = to_new_max;

    SOS_TIME(stop_time);

    dlog(6, "  ... done.  (ALLOC: %3.6lf seconds\n", (stop_time - start_time));

    return;
}



/**
 * @brief Internal utility function to see if a file exists.
 * @return 1 == file exists, 0 == file does not exist.
 */
int SOS_file_exists(char *filepath) {
    struct stat   buffer;
    return (stat(filepath, &buffer) == 0);
}

void SOS_str_strip_ext(char *str) {
    int i, len;
    len = strlen(str);
    for (i = 0; i < len; i++) {
        if (str[i] == '\"') str[i] = '\'';
        if (str[i] < ' ' || str[i] > '~') str[i] = '#';
    }
    return;
}

void SOS_str_to_upper(char *mutable_str) {
    if (mutable_str == NULL) return;
    char *c = mutable_str;
    while (*c) {
        *c = toupper((unsigned char)*c);
        c++;
    }
    return;
}

bool SOS_str_opt_is_enabled(char *mutable_str) {
    char *opt_str = mutable_str;
    if (opt_str == NULL) return false;
    SOS_str_to_upper(opt_str);
    if ((strcmp(opt_str, "1")           == 0) ||
        (strcmp(opt_str, "TRUE")        == 0) ||
        (strcmp(opt_str, "YES")         == 0) ||
        (strcmp(opt_str, "ENABLED")     == 0) ||
        (strcmp(opt_str, "VERBOSE")     == 0) ||
        (strcmp(opt_str, "ON")          == 0)) {
        return true;
    } else {
        return false;
    }
}

bool SOS_str_opt_is_disabled(char *mutable_str) {
    char *opt_str = mutable_str;
    if (opt_str == NULL) return false;
    SOS_str_to_upper(opt_str);
    if ((strcmp(opt_str, "0")           == 0) ||
        (strcmp(opt_str, "-1")          == 0) ||
        (strcmp(opt_str, "FALSE")       == 0) ||
        (strcmp(opt_str, "NO")          == 0) ||
        (strcmp(opt_str, "DISABLED")    == 0) ||
        (strcmp(opt_str, "OFF")         == 0)) {
        return true;
    } else {
        return false;
    }
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



int
SOS_pack(SOS_pub *pub, const char *name,
        SOS_val_type type, const void *val)
{
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack");
    int rc = -1;

    SOS_val_snap *snap = calloc(1, sizeof(SOS_val_snap));
    pthread_mutex_lock(pub->lock);
   
    rc = SOS_pack_snap_situate_in_pub(pub, snap, name, type, val);
    if (rc < 0) { return rc; }

    rc = SOS_pack_snap_renew_pub_data(pub, snap); if (rc < 0) { return rc; } 
    rc = SOS_pack_snap_add_to_pub_cache(pub, snap); if (rc < 0) { return rc; }
    rc = SOS_pack_snap_into_val_queue(pub, snap); if (rc < 0) { return rc; }
    
    pthread_mutex_unlock(pub->lock);
    return snap->elem;
}

int
SOS_pack_related(SOS_pub *pub, long relation_id, const char *name,
        SOS_val_type type, const void *val)
{
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack");
    int rc = -1;

    SOS_val_snap *snap = calloc(1, sizeof(SOS_val_snap));
    pthread_mutex_lock(pub->lock);

    rc = SOS_pack_snap_situate_in_pub(pub, snap, name, type, val);
    if (rc < 0) { return rc; }

    // Apply additional metadata:
    //
    snap->relation_id = relation_id;
    //

    rc = SOS_pack_snap_renew_pub_data(pub, snap); if (rc < 0) { return rc; } 
    rc = SOS_pack_snap_add_to_pub_cache(pub, snap); if (rc < 0) { return rc; }
    rc = SOS_pack_snap_into_val_queue(pub, snap); if (rc < 0) { return rc; }

    pthread_mutex_unlock(pub->lock);
    return snap->elem;
}


int SOS_pack_snap_situate_in_pub(SOS_pub *pub, SOS_val_snap *snap,
        const char *name, SOS_val_type type, const void *val)
{
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack_snap_situate_in_pub");   


    switch(type) {
    case SOS_VAL_TYPE_INT:    snap->val.i_val = *(int *)val;    break;
    case SOS_VAL_TYPE_LONG:   snap->val.l_val = *(long *)val;   break;
    case SOS_VAL_TYPE_DOUBLE: snap->val.d_val = *(double *)val; break;
    case SOS_VAL_TYPE_STRING: snap->val.c_val = strdup((char *)val);
                              snap->val_len   = strlen(snap->val.c_val);
                              break;
    case SOS_VAL_TYPE_BYTES:
        fprintf(stderr, "WARNING: SOS_pack(...) used to pack SOS_VAL_TYPE_BYTES."
                " This is unsupported.\n");
        fprintf(stderr, "WARNING: Please use SOS_pack_bytes(...) instead!\n");
        fprintf(stderr, "WARNING: Doing nothing and returning....\n");
        fflush(stderr);
        snap->elem = -1;
        return -1;
        break;

    default:
        dlog(0, "ERROR: Invalid type sent to SOS_pack."
                " (%d)\n", (int) type);
        return -1;
        break;
    }

    // NOTE: Regarding indexing...
    // The hash table will return NULL if a value is not present.
    // The pub->data[elem] index is zero-indexed, so indices are stored +1, to
    //   differentiate between empty and the first position.  The value
    //   returned by SOS_pub_search() is the actual array index to be used.
    int pos = SOS_pub_search(pub, name);

    SOS_data *data;

    if (pos < 0) {
        // Value does NOT EXIST in the pub.
        // Check if we need to expand the pub
        if (pub->elem_count >= pub->elem_max) {
            SOS_expand_data(pub);
        }

        // Force a pub announce at the next SOS_publish().
        pub->announced = 0;

        // Add this new value [name] to the pub...
        pos = pub->elem_count;
        pub->elem_count++;

        // REMINDER: (pos + 1) is correct.
        // We're storing it's "N'th element" position
        // rather than it's array index.
        // See SOS_pub_search(...) for details.
        pub->name_table->put(pub->name_table, name,
                (void *) ((long)(pos + 1)));

        data = pub->data[pos];

        // Set some defaults. These will get updated later...
        data->type  = type;
        data->guid  = SOS_uid_next(SOS->uid.my_guid_pool);
        data->val.c_val = NULL;
        data->val_len = 0;
        strncpy(data->name, name, SOS_DEFAULT_STRING_LEN);

    } else {
        // Name ALREADY EXISTS in the pub...
        data = pub->data[pos];
    }

    snap->elem        = pos;
    snap->guid        = data->guid;
    snap->pub_guid    = pub->guid;
    snap->frame       = pub->frame;
    snap->type        = data->type;

    // The value has already been put in the snap at the top of the function.
    // We leave with the correct placement and guid of this value, and a snap
    //     that is partially filled in, but holds the new value and type.

    return snap->elem;
} //end function: SOS_pack_snap_situate_in_pub()


int SOS_pack_snap_renew_pub_data(SOS_pub *pub, SOS_val_snap *snap) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack_renew_pub_data"); 

    // Update the value in the pub->data[elem] position.
    SOS_data *data = pub->data[snap->elem]; 
    
    switch(snap->type) {

    case SOS_VAL_TYPE_STRING:
        if (data->val.c_val != NULL) {
            free(data->val.c_val);
        }
        if (snap->val.c_val != NULL) {
            data->val.c_val = strndup(snap->val.c_val, SOS_DEFAULT_STRING_LEN);
            data->val_len   = strlen(snap->val.c_val);
        } else {
            dlog(0, "WARNING: You packed a null value for pub(%s)->data[%d]!\n",
                 pub->title, snap->elem);
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
        data->val.i_val = snap->val.i_val;
        data->val_len   = sizeof(int);
        break;

    case SOS_VAL_TYPE_LONG:
        data->val.l_val = snap->val.l_val;
        data->val_len   = sizeof(long);
        break;

    case SOS_VAL_TYPE_DOUBLE:
        data->val.d_val = snap->val.d_val;
        data->val_len   = sizeof(double);
        break;

    default:
        dlog(0, "ERROR: Invalid data type was specified.   (%d)\n", data->type);
        exit(EXIT_FAILURE);

    }

    data->state = SOS_VAL_STATE_DIRTY;
    SOS_TIME( data->time.pack );
    snap->time.pack = data->time.pack;

    return snap->elem;
}

int SOS_pack_snap_list_into_pub_cache(SOS_pub *pub, SOS_val_snap **snap_list) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack_snap_list_into_pub_cache");

    // MUTEX NOTE: This code assumes the pub->lock and global cache
    //             mutexes have both been obtained by the calling
    //             context.

    // Sanity checks:
    if (pub->cache_depth < 1) { return 0; }
    if (snap_list == NULL) { return 0; }
    if (snap_list[0] == NULL) { return 0; }

    if (pub->meta.nature == SOS_NATURE_SOS) {
        // These values have been packed/stored in SOS already
        // because they were created by the daemon.
        // We should not have gotten here, but return anyway.
        return 0;
    }

    // Safely determine where the new list is going to get emplaced:
    int insert_pos = -1;
    if (pub->cache_head == 0) {
        // Loop back around to the end:
        insert_pos = (pub->cache_depth - 1);
    } else {
        insert_pos = (pub->cache_head - 1);
    }

    // Free up any existing entries before inserting the new list:
    SOS_val_snap *snap;
    SOS_val_snap *next_snap;

    snap = pub->cache[insert_pos]; 
    while (snap != NULL) {
       SOS_val_snap *next_snap = snap->next_snap;
       SOS_val_snap_destroy(&snap);
       snap = next_snap;
    }

    // Emplace this new list:
    pub->cache[insert_pos] = snap_list[0];

    // Make sure the pub handle reflects the largest observed frame:
    if (pub->frame < snap_list[0]->frame) {
        pub->frame = snap_list[0]->frame;
    }

    pub->cache_head = insert_pos;

    //Done.
    return 0;
}


int SOS_pack_snap_add_to_pub_cache(SOS_pub *pub, SOS_val_snap *snap) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack_snap_add_to_pub_cache"); 

    // MUTEX NOTE: It is assumed that the pub->lock and global cache
    //             mutex have already been obtained by the calling
    //             context.

    // Sanity checks:
    if (SOS->role == SOS_ROLE_CLIENT) { return snap->elem; }
    if (pub->cache_depth < 1) { return snap->elem; }

    if (pub->cache[0]->frame != snap->frame) {
        // Grrr... this should not happen.
        // TODO: Check the assumption that it's the new frame is > old...
        // TODO: snap->next_snap?
        // TODO: snap->prev_snap?
        // TODO: Not sure this is the behavior we're wanting...
        SOS_pack_snap_list_into_pub_cache(pub, &snap);
    }

    // NOTE: Daemons COPY the snap into this pub's cache for rapid query.
    //       This is done using a COPY of the snap rather than the same one
    //       that is placed in the snap_queue for database ingestion.
    //       because these two snaps have different lifecycles.
    dlog(8, "pub->cache_depth == %d\n", pub->cache_depth);
    // ...
    SOS_val_snap *snap_copy = 
        (SOS_val_snap *) calloc(1, sizeof(SOS_val_snap));
    // Copy all the static member values of the snap:
    memcpy(snap_copy, snap, sizeof(SOS_val_snap));
    // Strings and Bytes need to be alloc'ed and copied in seperately:
    switch (snap_copy->type) {
        case SOS_VAL_TYPE_STRING: 
            snap_copy->val.c_val =
                (char *) calloc(snap_copy->val_len, sizeof(char));
            memcpy(snap_copy->val.c_val, snap->val.c_val, snap_copy->val_len);
            break;
        case SOS_VAL_TYPE_BYTES:
            snap_copy->val.bytes =
                (void *) calloc(snap_copy->val_len, sizeof(unsigned char));
            memcpy(snap_copy->val.bytes, snap->val.bytes, snap_copy->val_len);
            break;
    }

    // Now that we have a copy, let's timestamp it:
    SOS_TIME(snap_copy->time.recv);

    // We have a new snap, push it down into the cache in the current frame.
    snap->next_snap = pub->cache[pub->cache_head];
    snap->prev_snap = NULL;
    //
    pub->cache[pub->cache_head] = snap;

    // Done.
    return snap->elem;
}

int SOS_pack_snap_into_val_queue(SOS_pub *pub, SOS_val_snap *snap) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack_snap_into_val_queue");

    pthread_mutex_lock(pub->snap_queue->sync_lock);
    pipe_push(pub->snap_queue->intake, (void *) &snap, 1);
    pub->snap_queue->elem_count++;
    pthread_mutex_unlock(pub->snap_queue->sync_lock);
    
    return snap->elem;
}

void SOS_val_snap_destroy(SOS_val_snap **snap_var) {
    SOS_val_snap *snap = *snap_var;
    
    if (snap == NULL) {
        return;
    }

    switch (snap->type) {
        case SOS_VAL_TYPE_STRING:
            if (snap->val.c_val != NULL) {
                free(snap->val.c_val);
            }
            break;
        case SOS_VAL_TYPE_BYTES:
            if (snap->val.bytes != NULL) {
                free(snap->val.bytes);
            }
            break;

    }
    memset(snap, 0, sizeof(SOS_val_snap));
    free(snap);
    *snap_var = NULL;
    return;
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
        //  ...is this still the case?
        return;
    }

    if (pub == NULL) { return; }

    _sos_lock_pub(pub,__func__);

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

    pthread_mutex_lock(pub->snap_queue->sync_lock);

    if (pub->snap_queue->elem_count < 1) {
        dlog(4, "  ... nothing to do for pub(%s)\n", pub->guid_str);
        pthread_mutex_unlock(pub->snap_queue->sync_lock);
        _sos_unlock_pub(pub,__func__);
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
    SOS_msg_zip(buffer, header, 0, &offset);

    SOS_buffer_pack(buffer, &offset, "i", snap_count);

    dlog(6, "     ... processing snaps extracted from the queue\n");

    for (snap_index = 0; snap_index < snap_count; snap_index++) {
        snap = snap_list[snap_index];

        dlog(6, "     ... guid=%" SOS_GUID_FMT "\n", snap->guid);

        SOS_TIME(snap->time.send);

        SOS_buffer_pack(buffer, &offset, "iggiiidddl",
                        snap->elem,
                        snap->guid,
                        snap->relation_id,
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
    SOS_msg_zip(buffer, header, 0, &offset);

    dlog(6, "     ... done   (buf_len == %d)\n", header.msg_size);

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
        dlog(2, "  ... skipping this request, potentially missing"
                " values in the daemon/database.\n");
        return;
    }

    dlog(6, "  ... building val_snap queue from a buffer:\n");
    dlog(6, "     ... processing header\n");

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

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

    // Snaps for the database:
    SOS_val_snap  *snap;
    SOS_val_snap **snap_list;
    
    // Snaps for the cache:
    SOS_val_snap  *snap_copy;
    SOS_val_snap **snap_copy_list;
   
    bool ynAddSnapsToCache =
        ((pub->cache_depth > 0)
      && (pub->meta.nature != SOS_NATURE_SOS));

    snap_list = (SOS_val_snap **) calloc(snap_count, sizeof(SOS_val_snap *));

    if (ynAddSnapsToCache) {
        snap_copy_list = (SOS_val_snap **) calloc(snap_count, sizeof(SOS_val_snap *));
    }

    for (snap_index = 0; snap_index < snap_count; snap_index++) {
        snap_list[snap_index]
                = (SOS_val_snap *) calloc(1, sizeof(SOS_val_snap));
        snap = snap_list[snap_index];

        snap->pub_guid = pub->guid;

        SOS_buffer_unpack(buffer, &offset, "iggiiidddl",
                          &snap->elem,
                          &snap->guid,
                          &snap->relation_id,
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
            // add one byte for the null terminator.
            snap->val.c_val = (char *) calloc(sizeof(char), snap->val_len+1);
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
        } // end: switch

        // Construct a duplicate list of snapshots for the cache:
        if (ynAddSnapsToCache) {
            snap_copy = calloc(1, sizeof(SOS_val_snap));
            memcpy(snap_copy, snap, sizeof(SOS_val_snap));
            switch (snap->type) {
            case SOS_VAL_TYPE_STRING:
                snap_copy->val.c_val = strdup(snap->val.c_val);
                break;
            case SOS_VAL_TYPE_BYTES:
                snap_copy->val.bytes = (void *)
                                       calloc(snap->val_len, sizeof(unsigned char));
                memcpy(snap_copy->val.bytes, snap->val.bytes, snap->val_len);
                break;
            } // end: switch
            SOS_TIME(snap_copy->time.recv);
            snap_copy_list[snap_index] = snap_copy;
        } // end: if (cache_depth)

    }// end for (snap_index)

    if (ynAddSnapsToCache) {
        // Link up the snap_copies in the array:
        for (snap_index = 0; snap_index < (snap_count - 1); snap_index++) {
            snap_copy_list[snap_index]->next_snap =
                snap_copy_list[snap_index + 1];
            
            if (snap_index > 0) {
                snap_copy_list[snap_index]->prev_snap =
                    snap_copy_list[snap_index - 1];
            }
        }
        // Have the head and tail snapshots point to NULL:
        snap_copy_list[0]->prev_snap = NULL;
        snap_copy_list[(snap_count - 1)]->next_snap = NULL;

        // Place these values in the pub->cache, if it is enabled:
        pthread_mutex_lock(SOS->task.global_cache_lock);
        //        
        SOS_pack_snap_list_into_pub_cache(pub, snap_copy_list);
        //
        pthread_mutex_unlock(SOS->task.global_cache_lock);

        // The individual snaps are now strung together as a linked list and
        // their head is referenced by the pub->cache handle, we can free
        // this explicitly defined list:
        free(snap_copy_list);
    }

    //
    // Place these snaps in the DB queue (if it is enabled):
    if (snap_queue != NULL) {
        // Place these snapshots in the next queue stage:
        dlog(6, "     ... pushing %d snaps down onto the queue.\n", snap_count);
        pthread_mutex_lock(snap_queue->sync_lock);
        pipe_push(snap_queue->intake, (void *) snap_list, snap_count);
        snap_queue->elem_count += snap_count;
        pthread_mutex_unlock( snap_queue->sync_lock );
    } else {
        // ELSE: There is NO further queue, so free all the snapshots:
        for (snap_index = 0; snap_index < snap_count; snap_index++) {
            snap = snap_list[snap_index];
            switch (snap->type) {
                case SOS_VAL_TYPE_INT:    snap->val.i_val = 0;   break;
                case SOS_VAL_TYPE_LONG:   snap->val.l_val = 0;   break;
                case SOS_VAL_TYPE_DOUBLE: snap->val.d_val = 0.0; break;
                case SOS_VAL_TYPE_STRING:
                                          if (snap->val.c_val != NULL) {
                                              free(snap->val.c_val);
                                              snap->val.c_val = NULL;
                                          }
                                          break;
                case SOS_VAL_TYPE_BYTES:
                                          if (snap->val.bytes != NULL) {
                                              free(snap->val.bytes);
                                              snap->val.bytes = NULL;
                                          }
                                          break;
            } // end: switch
        } // end: forall snaps
    } //end: if no requeue...

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

    // CONCURRENCY: This function assumes pub->lock is already held.
    
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ANNOUNCE;
    header.msg_from = SOS->my_guid;
    header.ref_guid = pub->guid;

    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    // Pub metadata.
    SOS_buffer_pack(buffer, &offset, "siiississiiiiiiii",
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
                    pub->meta.retain_hint,
                    pub->cache_depth);

    // Data definitions.
    for (elem = 0; elem < pub->elem_count; elem++) {
        SOS_buffer_pack(buffer, &offset, "gsiiiiiig",
                        pub->data[elem]->guid,
                        pub->data[elem]->name,
                        pub->data[elem]->type,
                        pub->data[elem]->meta.freq,
                        pub->data[elem]->meta.semantic,
                        pub->data[elem]->meta.classifier,
                        pub->data[elem]->meta.pattern,
                        pub->data[elem]->meta.compare,
                        pub->data[elem]->meta.relation_id );
    }

    pub->announced = 1;

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    return;
}


void SOS_publish_to_buffer(SOS_pub *pub, SOS_buffer *buffer) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish_to_buffer");
    SOS_msg_header   header;
    long             this_frame;
    double           send_time;
    int              offset;
    int              elem;

    // CONCURRENCY: This function assumes the pub->lock is already held.
    
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
    SOS_msg_zip(buffer, header, 0, &offset);

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

        SOS_buffer_pack(buffer, &offset, "iddilg",
                        elem,
                        pub->data[elem]->time.pack,
                        pub->data[elem]->time.send,
                        pub->data[elem]->val_len,
                        pub->data[elem]->meta.semantic,
                        pub->data[elem]->meta.relation_id);

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
    SOS_msg_zip(buffer, header, 0, &offset);

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
    SOS_msg_unzip(buffer, &header, 0, &offset);

    pub->guid = header.ref_guid;
    snprintf(pub->guid_str, SOS_DEFAULT_STRING_LEN,
            "%" SOS_GUID_FMT, pub->guid);

    dlog(6, "  ... unpacking the pub definition.\n");
    SOS_buffer_unpack(buffer, &offset, "siiississiiiiiiii",
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
        &pub->meta.retain_hint,
        &pub->cache_depth);

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
    dlog(6, "pub->cache_depth = %d\n", pub->cache_depth);

    // We shouldn't have a cache yet, so allocate it with the depth that
    // the user has requested. 
    if (pub->cache != NULL) {
        dlog(1, "WARNING: Handling a re-announcement for"
                " a pub with an existing cache.\n");
    } else {
        pub->cache = (SOS_val_snap **)
            calloc(pub->cache_depth, sizeof(SOS_val_snap *));
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

        SOS_buffer_unpack(buffer, &offset, "gsiiiiiig",
            &upd_elem.guid,
            upd_elem.name,
            &upd_elem.type,
            &upd_elem.meta.freq,
            &upd_elem.meta.semantic,
            &upd_elem.meta.classifier,
            &upd_elem.meta.pattern,
            &upd_elem.meta.compare,
            &upd_elem.meta.relation_id );

        if ((   pub->data[elem]->guid             != upd_elem.guid )
            || (pub->data[elem]->type             != upd_elem.type )
            || (pub->data[elem]->meta.freq        != upd_elem.meta.freq )
            || (pub->data[elem]->meta.semantic    != upd_elem.meta.semantic )
            || (pub->data[elem]->meta.classifier  != upd_elem.meta.classifier )
            || (pub->data[elem]->meta.pattern     != upd_elem.meta.pattern )
            || (pub->data[elem]->meta.compare     != upd_elem.meta.compare )
            || (pub->data[elem]->meta.relation_id != upd_elem.meta.relation_id )) {

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

            pub->data[elem]->guid             = upd_elem.guid;
            pub->data[elem]->type             = upd_elem.type;
            pub->data[elem]->meta.freq        = upd_elem.meta.freq;
            pub->data[elem]->meta.semantic    = upd_elem.meta.semantic;
            pub->data[elem]->meta.classifier  = upd_elem.meta.classifier;
            pub->data[elem]->meta.pattern     = upd_elem.meta.pattern;
            pub->data[elem]->meta.compare     = upd_elem.meta.compare;
            pub->data[elem]->meta.relation_id = upd_elem.meta.relation_id;

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

    pthread_mutex_lock(pub->lock);

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

    dlog(7, "Unpacking the values from the buffer...\n");

    offset = 0;
    SOS_msg_unzip(buffer, &header, 0, &offset);

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

        SOS_buffer_unpack(buffer, &offset, "ddilg",
                          &data->time.pack,
                          &data->time.send,
                          &data->val_len,
                          &data->meta.semantic,
                          &data->meta.relation_id);

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

    dlog(6, "Preparing an announcement message...\n");

    if (pub->announced != 0) {
        dlog(3, "This publication has already been announced!"
                " Doing nothing.\n");
        return;
    }

    ann_buf = NULL;
    rep_buf = NULL;
    SOS_buffer_init(SOS, &ann_buf);
    SOS_buffer_init_sized(SOS, &rep_buf, SOS_DEFAULT_REPLY_LEN);

    pthread_mutex_lock(pub->lock);

    dlog(6, "  ... placing the announce message in a buffer.\n");
    SOS_announce_to_buffer(pub, ann_buf);

    pthread_mutex_unlock(pub->lock);

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

    pub_buf = NULL;
    rep_buf = NULL;
    SOS_buffer_init(SOS, &pub_buf);
    SOS_buffer_init_sized(SOS, &rep_buf, SOS_DEFAULT_REPLY_LEN);
    
    // CONCURRENCY: Announce will lock the pub, so don't lock it yet...

    if (pub->announced == 0) {
        dlog(6, "AUTO-ANNOUNCING this pub...\n");
        SOS_announce(pub);
    }

    pthread_mutex_lock(pub->lock);

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

    pthread_mutex_unlock(pub->lock);

    return;
}


