#ifndef SOS_H
#define SOS_H

/*
 * sos.h              Core API for SOS_flow project. 
 *
 *                    (see also:  sos_types.h)
 *
 */


// NOTE: Major and minor versions should be simple integers,
//       as they are treated as ints when exchanged between
//       client and server.

#define SOS_VERSION_MAJOR 0
#define SOS_VERSION_MINOR 11 

// ...

#define SOS_Q(x) #x
#define SOS_QUOTE_DEF_STR(x) SOS_Q(x)

#ifndef SOS_BUILDER
#define SOS_BUILDER "----------" 
#endif

#ifndef SOS_BUILT_FOR
#define SOS_BUILT_FOR "----------"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>


/* SOS Configuration Switches... */

#define SOS_CONFIG_DB_STRING_ENUMS  0
#define SOS_CONFIG_USE_THREAD_POOL  1
#define SOS_CONFIG_FEEDBACK_ACTIVE  1

#define SOS_DEFAULT_SERVER_HOST     "localhost"
#define SOS_DEFAULT_SERVER_PORT     "22500"
#define SOS_DEFAULT_MSG_TIMEOUT     2048
#define SOS_DEFAULT_TIMEOUT_SEC     2.0
#define SOS_DEFAULT_BUFFER_MAX      4096
#define SOS_DEFAULT_BUFFER_MIN      512
#define SOS_DEFAULT_PIPE_DEPTH      100000
#define SOS_DEFAULT_REPLY_LEN       128
#define SOS_DEFAULT_FEEDBACK_LEN    1024
#define SOS_DEFAULT_STRING_LEN      256
#define SOS_DEFAULT_RING_SIZE       65536
#define SOS_DEFAULT_TABLE_SIZE      655360
#define SOS_DEFAULT_GUID_BLOCK      8001027
#define SOS_DEFAULT_ELEM_MAX        1024
#define SOS_DEFAULT_UID_MAX         LLONG_MAX


#include "sos_types.h"


/* ************************************ */
/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif


    // ---------- primary functions --------------------

    void SOS_init(int *argc, char ***argv,
        SOS_runtime **runtime, SOS_role role,
        SOS_receives receives, SOS_feedback_handler_f handler);

    void SOS_pub_create(SOS_runtime *sos_context,
        SOS_pub **pub_handle, char *pub_title, SOS_nature nature);

    int SOS_pack(SOS_pub *pub, const char *name,
        SOS_val_type pack_type, void *pack_val_var);

    int SOS_pack_bytes(SOS_pub *pub, const char *name,
        int byte_count, void *pack_source);

    int SOS_event(SOS_pub *pub, const char *name,
        SOS_val_semantic semantic);

    void SOS_announce(SOS_pub *pub);

    void SOS_publish(SOS_pub *pub);

    void SOS_sense_register(SOS_runtime *sos_context, char *handle);

    void SOS_sense_trigger(SOS_runtime *sos_context,
        char *handle, char *data, int data_length);

    void SOS_finalize(SOS_runtime *sos_context);




    // ---------- internal / utility functions -----------------

    void SOS_init_existing_runtime(int *argc, char ***argv,
        SOS_runtime **runtime, SOS_role role,
        SOS_receives receives, SOS_feedback_handler_f handler);

    int SOS_file_exists(char *path);

    void SOS_pub_create_sized(SOS_runtime *sos_context, SOS_pub **pub_handle,
        char *pub_title, SOS_nature nature, int new_size);

    int SOS_pub_search(SOS_pub *pub, const char *name);

    void SOS_pub_destroy(SOS_pub *pub);

    void SOS_announce_to_buffer(SOS_pub *pub, SOS_buffer *buffer);

    void SOS_announce_from_buffer(SOS_buffer *buffer, SOS_pub *pub);

    void SOS_publish_to_buffer(SOS_pub *pub, SOS_buffer *buffer);

    void SOS_publish_from_buffer(SOS_buffer *buffer,
        SOS_pub *pub, SOS_pipe *optional_snap_queue);

    void SOS_uid_init(SOS_runtime *sos_context,
        SOS_uid **uid, SOS_guid from, SOS_guid to);

    SOS_guid SOS_uid_next(SOS_uid *uid);

    void SOS_uid_destroy(SOS_uid *uid);

    void SOS_val_snap_queue_to_buffer(SOS_pub *pub,
        SOS_buffer *buffer, bool destroy_snaps);

    void SOS_val_snap_queue_from_buffer(SOS_buffer *buffer,
        SOS_pipe *snap_queue, SOS_pub *pub);

    void SOS_strip_str(char *str);

    char* SOS_uint64_to_str(uint64_t val, char *result, int result_len);

    // Communication wrapper functions:

    int SOS_msg_zip(SOS_buffer *msg, int msg_length, int at_offset);

    int SOS_msg_unzip(SOS_buffer *msg, SOS_msg_header *header,
            int *offset_after_header);

    int SOS_target_init(SOS_runtime *sos_context, SOS_socket_out **target,
            char *host, int port);

    int SOS_target_connect(SOS_socket_out *target);

    int SOS_target_send_msg(SOS_socket_out *target, SOS_buffer *msg);

    int SOS_target_recv_msg(SOS_socket_out *target, SOS_buffer *reply);

    int SOS_target_disconnect(SOS_socket_out *tgt_conn);

    int SOS_target_destroy(SOS_socket_out *target);

    //Soon deprecated...
    void SOS_send_to_daemon(SOS_buffer *buffer, SOS_buffer *reply);


#ifdef __cplusplus
}
#endif

#ifndef SOS_max
#define SOS_max(a,b) ((a > b) ? a : b)
#endif

#ifndef SOS_min
#define SOS_min(a,b) ((a < b) ? a : b)
#endif

#define SOS_TIME(__SOS_now)       { struct timeval t; gettimeofday(&t, NULL); __SOS_now = (double)(t.tv_sec + (t.tv_usec/1e6)); }


#define SOS_LOCK_REENTRANT(__SOS_int_var, usec_delay)  {        \
    timespec __SOS_spinlock_ts;                                 \
    __SOS_spinlock_ts.tv_sec  = 0;                              \
    __SOS_spinlock_ts.tv_usec = usec_delay;                     \
    while (__SOS_int) {                                         \
        nanosleep(__SOS_spinlock_ts, NULL)                      \
    }                                                           \
    __SOS_int_var += 1;                                         \
    }

#define SOS_UNLOCK_REENTRANT(__SOS_int_var) {           \
    __SOS_int_var -= 1;                                 \
    }

#if (SOS_DEBUG < 0)
    #define SOS_SET_CONTEXT(__SOS_context, __SOS_str_func)              \
    SOS_runtime *SOS;                                                   \
    SOS = (SOS_runtime *) __SOS_context;                                \
    if (SOS == NULL) {                                                  \
        printf("(%s:%s) ERROR: SOS_runtime *sos_context provided to SOS_SET_CONTEXT() is null!\n", \
               __FILE__, __LINE__);                                     \
        exit(EXIT_FAILURE);                                             \
    }
#else
#define SOS_SET_CONTEXT(__SOS_context, __SOS_str_funcname)                                                                 \
    SOS_runtime *SOS;                                                                                                      \
    SOS = (SOS_runtime *) __SOS_context;                                                                                   \
    if (SOS == NULL) {                                                                                                     \
                      printf("ERROR: SOS_runtime *sos_context provided to SOS_SET_CONTEXT() is null!  (%s)\n",             \
                             __SOS_str_funcname);                                                                          \
                      exit(EXIT_FAILURE);                                                                                  \
                      }                                                                                                    \
    char SOS_WHOAMI[SOS_DEFAULT_STRING_LEN] = {0};                                                                         \
    snprintf(SOS_WHOAMI, SOS_DEFAULT_STRING_LEN, "* ??? *");                                                               \
    switch (SOS->role) {                                                                                                   \
    case SOS_ROLE_CLIENT     : sprintf(SOS_WHOAMI, "client(%" SOS_GUID_FMT ").%s",  SOS->my_guid, __SOS_str_funcname); break;          \
    case SOS_ROLE_LISTENER   : sprintf(SOS_WHOAMI, "listener(%d).%s",                 SOS->config.comm_rank, __SOS_str_funcname); break; \
    case SOS_ROLE_AGGREGATOR : sprintf(SOS_WHOAMI, "aggregator(%d).%s",                     SOS->config.comm_rank, __SOS_str_funcname); break; \
    case SOS_ROLE_ANALYTICS  : sprintf(SOS_WHOAMI, "analytics(%d).%s",              SOS->config.comm_rank, __SOS_str_funcname); break; \
    default                  : sprintf(SOS_WHOAMI, "------(%" SOS_GUID_FMT ").%s",  SOS->my_guid, __SOS_str_funcname); break;          \
    }
#endif




#endif //SOS_H
