#ifndef SOS_H
#define SOS_H

/*
 * sos_stub.h
 *
 * Drop-in API replacement that macros-out SOS functions.
 *
 * This is meant for USERS, as the internal library functions
 * may have more complex embeddings. It doesn't make sense
 * to be macro-ing out SOS from within SOS, but for consistency
 * we knock out all the function signatures anyway.
 *
 * NOTE: Modify this to suite your use cases.  Some users collect
 *       SOS_pack(...), returned pub index value so will need to
 *       replace its ';;;' macro with '-99999' or equivalent.
 */


#define SOS_VERSION_MAJOR 0
#define SOS_VERSION_MINOR 0 

// ...

#ifndef SOS_BUILDER
#define SOS_BUILDER "----------" 
#endif

#ifndef SOS_BUILT_FOR
#define SOS_BUILT_FOR "----------"
#endif

/* SOS Configuration Switches... */

#define SOS_CONFIG_DB_STRING_ENUMS  -99999 
#define SOS_CONFIG_USE_THREAD_POOL  -99999
#define SOS_CONFIG_FEEDBACK_ACTIVE  -99999

#define SOS_DEFAULT_SERVER_HOST     ""
#define SOS_DEFAULT_SERVER_PORT     ""
#define SOS_DEFAULT_MSG_TIMEOUT     -99999
#define SOS_DEFAULT_TIMEOUT_SEC     -99999
#define SOS_DEFAULT_BUFFER_MAX      -99999
#define SOS_DEFAULT_BUFFER_MIN      -99999
#define SOS_DEFAULT_PIPE_DEPTH      -99999
#define SOS_DEFAULT_REPLY_LEN       -99999
#define SOS_DEFAULT_FEEDBACK_LEN    -99999
#define SOS_DEFAULT_STRING_LEN      -99999
#define SOS_DEFAULT_RING_SIZE       -99999
#define SOS_DEFAULT_TABLE_SIZE      -99999
#define SOS_DEFAULT_GUID_BLOCK      -99999
#define SOS_DEFAULT_ELEM_MAX        -99999
#define SOS_DEFAULT_UID_MAX         -99999


#ifdef __cplusplus
extern "C" {
#endif

// =======================================================
// ==================== SOS FUNCTIONS ====================
// =======================================================

// --- SOS core API (EXTERNAL):
#define SOS_init(...)                               ;;; 
#define SOS_pub_create(...)                         ;;;
#define SOS_pack(...)                               ;;; 
#define SOS_pack_bytes(...)                         ;;;
#define SOS_event(...)                              ;;;
#define SOS_announce(...)                           ;;;
#define SOS_publish(...)                            ;;;
#define SOS_sense_register(...)                     ;;;
#define SOS_sense_trigger(...)                      ;;;
#define SOS_finalize(...)                           ;;;

// --- SOS utilities (INTERNAL):
#define SOS_init_existing_runtime(...)              ;;;
#define SOS_process_options_file(...)               ;;;
#define SOS_file_exists(...)                        -99999 
#define SOS_pub_create_sized(...)                   ;;;
#define SOS_pub_search(...)                         -99999
#define SOS_pub_destroy(...)                        ;;;
#define SOS_announce_to_buffer(...)                 ;;;
#define SOS_announce_from_buffer(...)               ;;;
#define SOS_publish_to_buffer(...)                  ;;;
#define SOS_publish_from_buffer(...)                ;;;
#define SOS_uid_init(...)                           ;;;
#define SOS_uid_next(...)                           99999 
#define SOS_uid_destroy(...)                        ;;;
#define SOS_val_snap_queue_to_buffer(...)           ;;;
#define SOS_val_snap_queue_from_buffer(...)         ;;;
#define SOS_strip_str(...)                          ;;;
#define SOS_uint64_to_str(...)                      ;;;

// --- SOS messaging:   (INTERNAL) 
#define SOS_msg_zip(...)                            -99999
#define SOS_msg_unzip(...)                          -99999
#define SOS_msg_seal(...)                           -99999
#define SOS_target_init(...)                        -99999
#define SOS_target_connect(...)                     -99999
#define SOS_target_accept_connection(...)           -99999
#define SOS_target_send_msg(...)                    -99999
#define SOS_target_recv_msg(...)                    -99999
#define SOS_target_disconnect(...)                  -99999
#define SOS_target_destroy(...)                     -99999
#define SOS_send_to_daemon(...)                     -99999

// =======================================================
// ==================== SOS FUNCTIONS ====================
// =======================================================



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
        printf("(%s:%s) ERROR: SOS_runtime *sos_context"        \
                " provided to SOS_SET_CONTEXT() is null!\n",    \
               __FILE__, __LINE__);                             \
        exit(EXIT_FAILURE);                                     \
    }
#else
#define SOS_SET_CONTEXT(__SOS_context, __SOS_str_funcname)      \
    SOS_runtime *SOS;                                           \
    SOS = (SOS_runtime *) __SOS_context;                        \
    if (SOS == NULL) {                                          \
        printf("ERROR: SOS_runtime *sos_context provided"       \
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


#endif
