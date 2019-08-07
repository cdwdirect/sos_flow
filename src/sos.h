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

#define SOS_VERSION_MAJOR 1
#define SOS_VERSION_MINOR 25 

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

#ifdef USE_MUNGE
#include <munge.h>
#endif

/* SOS Configuration Switches... */

#define SOS_CONFIG_DB_STRING_ENUMS  0
#define SOS_CONFIG_USE_THREAD_POOL  1
#define SOS_CONFIG_FEEDBACK_ACTIVE  1

#define SOS_DEFAULT_SERVER_HOST     "127.0.0.1"
#define SOS_DEFAULT_SERVER_PORT     "22500"
#define SOS_DEFAULT_MSG_TIMEOUT     2048
#define SOS_DEFAULT_TIMEOUT_SEC     2.0
#define SOS_DEFAULT_BUFFER_MAX      4096
#define SOS_DEFAULT_BUFFER_MIN      512
#define SOS_DEFAULT_PIPE_DEPTH      100000
#define SOS_DEFAULT_REPLY_LEN       1024
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

    void SOS_init(
        SOS_runtime **runtime, SOS_role role,
        SOS_receives receives, SOS_feedback_handler_f handler);

    void SOS_pub_init(SOS_runtime *sos_context,
        SOS_pub **pub_handle, const char *pub_title, SOS_nature nature);

    void SOS_pub_config(SOS_pub *pub, SOS_pub_option opt, ...);

    int SOS_pack(SOS_pub *pub, const char *name,
        SOS_val_type pack_type, const void *pack_val_var);

    int SOS_pack_related(SOS_pub *pub, long relation_id, const char *name,
        SOS_val_type pack_type, const void *pack_val_var);

    // NOTE: This seems like a nice idea.  Soon.  -CDW
    //int SOS_pack_array(SOS_pub *pub, const char **names,
    //        int elem_count, SOS_val_type pack_type, const void *array);

    void SOS_announce(SOS_pub *pub);

    void SOS_publish(SOS_pub *pub);

    void SOS_sense_register(SOS_runtime *sos_context, const char *handle);

    void SOS_sense_trigger(SOS_runtime *sos_context,
        const char *handle, const char *data, int data_length);


    void SOS_finalize(SOS_runtime *sos_context);



    // ---------- internal / utility functions -----------------

    void SOS_init_remote(
        SOS_runtime **runtime, const char *remote_hostname, SOS_role role,
        SOS_receives receives, SOS_feedback_handler_f handler);

    void SOS_init_existing_runtime(
        SOS_runtime **runtime, SOS_role role,
        SOS_receives receives, SOS_feedback_handler_f handler);

    int SOS_file_exists(char *path);
    int SOS_dir_exists(char *path);

    void SOS_pub_init_sized(SOS_runtime *sos_context, SOS_pub **pub_handle,
        const char *pub_title, SOS_nature nature, int new_size);


    // NOTE: Sub-components of the SOS_pack() API call, allowing code
    //       reuse among the different interactions like SOS_pack() VS.
    //       SOS_pack_related() and additional future data inlets:
    //
    // Find where in the pub this new value should go, and populate the
    // SOS_val_snap *snap obj representing with the values and position.
    int SOS_pack_snap_situate_in_pub(SOS_pub *pub, SOS_val_snap *snap,
            const char *name, SOS_val_type type, const void *val);
    //
    // Update the pub->data[elem] values using this snapshot.
    int SOS_pack_snap_renew_pub_data(SOS_pub *pub, SOS_val_snap *snap);
    //
    // Add a single snap to the latest cache entry:
    int SOS_pack_snap_add_to_pub_cache(SOS_pub *pub, SOS_val_snap *snap);
    //
    // Add a list of snapshots from digesting a buffer sent in a publish:
    // (This will increment the pub->latest_frame and move around
    // the cache ring buffer.)
    int SOS_pack_snap_list_into_pub_cache(SOS_pub *pub, SOS_val_snap **snap_list);
    //
    // This puts an individual snapshot into the "next steps" queue,
    // or the "queue to send to the daemon" for clients, for example:
    int SOS_pack_snap_into_val_queue(SOS_pub *pub, SOS_val_snap *snap);
    // -----

    void  SOS_reference_set(SOS_runtime *sos_context, const char *name, void *pointer);
    void* SOS_reference_get(SOS_runtime *sos_context, const char *name);

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

    void SOS_val_snap_destroy(SOS_val_snap **snap_var);

    void SOS_str_strip_ext(char *str);
    void SOS_str_to_upper(char *mutable_str);
    bool SOS_str_opt_is_enabled(char *mutable_str);
    bool SOS_str_opt_is_disabled(char *mutable_str);

    char* SOS_uint64_to_str(uint64_t val, char *result, int result_len);

    // Communication wrapper functions:

    int SOS_msg_zip(SOS_buffer *msg, SOS_msg_header header,
            int starting_offset, int *offset_after_header);

    int SOS_msg_unzip(SOS_buffer *msg, SOS_msg_header *header,
            int starting_offset, int *offset_after_header);

    int SOS_msg_seal(SOS_buffer *msg, SOS_msg_header header,
            int starting_offset, int *offset_after_header_size_field);

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

#define SOS_TIME(__SOS_now)                                     \
    {                                                           \
        struct timeval t;                                       \
        gettimeofday(&t, NULL);                                 \
        __SOS_now = (double)(t.tv_sec + (t.tv_usec/1e6));       \
    }


#define SOS_LOCK_REENTRANT(__SOS_int_var, usec_delay)  {        \
    timespec __SOS_spinlock_ts;                                 \
    __SOS_spinlock_ts.tv_sec  = 0;                              \
    __SOS_spinlock_ts.tv_usec = usec_delay;                     \
    while (__SOS_int) {                                         \
        nanosleep(__SOS_spinlock_ts, NULL)                      \
    }                                                           \
    __SOS_int_var += 1;                                         \
    }

#define SOS_UNLOCK_REENTRANT(__SOS_int_var) {                   \
    __SOS_int_var -= 1;                                         \
    }

#if (SOS_DEBUG < 0)
    #define SOS_SET_CONTEXT(__SOS_context, __SOS_str_func)      \
    SOS_runtime *SOS;                                           \
    SOS = (SOS_runtime *) __SOS_context;                        \
    if (SOS == NULL) {                                          \
        fprintf(stderr, "(%s:%s) WARNING: SOS_runtime *sos_context" \
                " provided to SOS_SET_CONTEXT() is null!\n",    \
               __FILE__, __LINE__);                             \
        exit(EXIT_FAILURE);                                     \
    }
#else
#define SOS_SET_CONTEXT(__SOS_context, __SOS_str_funcname)      \
    SOS_runtime *SOS;                                           \
    SOS = (SOS_runtime *) __SOS_context;                        \
    if (SOS == NULL) {                                          \
        fprintf(stderr, "ERROR: SOS_runtime *sos_context provided" \
                " provided to SOS_SET_CONTEXT() is null!"       \
                "  (%s)\n", __SOS_str_funcname);                \
        exit(EXIT_FAILURE);                                     \
    }                                                           \
    char SOS_WHOAMI[SOS_DEFAULT_STRING_LEN] = {0};              \
    char SOS_WHEREAMI[SOS_DEFAULT_STRING_LEN] = {0};            \
    snprintf(SOS_WHOAMI, SOS_DEFAULT_STRING_LEN, "* ??? *");    \
    switch (SOS->role) {                                        \
    case SOS_ROLE_CLIENT     :                                  \
        sprintf(SOS_WHOAMI, "client(%" SOS_GUID_FMT ")",        \
                SOS->my_guid);                                  \
        break;                                                  \
    case SOS_ROLE_LISTENER   :                                  \
        sprintf(SOS_WHOAMI, "listener(%d)",                     \
                SOS->config.comm_rank);                         \
        break;                                                  \
    case SOS_ROLE_AGGREGATOR :                                  \
        sprintf(SOS_WHOAMI, "aggregator(%d)",                   \
                SOS->config.comm_rank);                         \
        break;                                                  \
    case SOS_ROLE_ANALYTICS  :                                  \
        sprintf(SOS_WHOAMI, "analytics(%d)",                    \
                SOS->config.comm_rank);                         \
        break;                                                  \
    default                  :                                  \
        sprintf(SOS_WHOAMI, "------(%" SOS_GUID_FMT ")",        \
                SOS->my_guid);                                  \
        break;                                                  \
    }                                                           \
    snprintf(SOS_WHEREAMI, SOS_DEFAULT_STRING_LEN, "%s",        \
            __SOS_str_funcname);                                \
    /*dlog(8, "Entering function: %s\n", __SOS_str_funcname);*/
#endif

// Unicode box drawing macros:
#define SOS_SYM_BR "\e(0\x6a\e(B " /* -^  */
#define SOS_SYM_TR "\e(0\x6b\e(B " /* -.  */
#define SOS_SYM_TL "\e(0\x6c\e(B " /* .-  */
#define SOS_SYM_BL "\e(0\x6d\e(B " /* ^-  */
#define SOS_SYM_LX "\e(0\x6e\e(B " /* -|- */
#define SOS_SYM_LH "\e(0\x71\e(B " /* --- */
#define SOS_SYM_VL "\e(0\x74\e(B " /*  |- */
#define SOS_SYM_VR "\e(0\x75\e(B " /* -|  */
#define SOS_SYM_LU "\e(0\x76\e(B " /* -^- */
#define SOS_SYM_LD "\e(0\x77\e(B " /* -.- */
#define SOS_SYM_LV "\e(0\x78\e(B " /*  |  */

// Symbols:
#define SOS_SYM_GREY_BLOCK "\xE2\x96\x92"

// Colors
#define SOS_RED         "\x1B[31m"
#define SOS_GRN         "\x1B[32m"
#define SOS_YEL         "\x1B[33m"
#define SOS_BLU         "\x1B[34m"
#define SOS_MAG         "\x1B[35m"
#define SOS_CYN         "\x1B[36m"
#define SOS_WHT         "\x1B[37m"
#define SOS_BOLD_RED    "\x1B[1;31m"
#define SOS_BOLD_GRN    "\x1B[1;32m"
#define SOS_BOLD_YEL    "\x1B[1;33m"
#define SOS_BOLD_BLU    "\x1B[1;34m"
#define SOS_BOLD_MAG    "\x1B[1;35m"
#define SOS_BOLD_CYN    "\x1B[1;36m"
#define SOS_BOLD_WHT    "\x1B[1;37m"
#define SOS_DIM_RED    "\x1B[2;31m"
#define SOS_DIM_GRN    "\x1B[2;32m"
#define SOS_DIM_YEL    "\x1B[2;33m"
#define SOS_DIM_BLU    "\x1B[2;34m"
#define SOS_DIM_MAG    "\x1B[2;35m"
#define SOS_DIM_CYN    "\x1B[2;36m"
#define SOS_DIM_WHT    "\x1B[2;37m"
#define SOS_CLR         "\x1B[0m"

// Example of colors:
// printf(SOS_RED "red\n" SOS_CLR);

#define SOS_TODO(__msg_str)                                     \
        fprintf(stderr, SOS_GRN ">>>" SOS_CLR                   \
                " " SOS_BOLD_GRN "TODO" SOS_CLR ": %s",         \
                __msg_str); fflush(stderr);


#endif
