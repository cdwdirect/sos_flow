#ifndef SSOS_H
#define SSOS_H

/*
 * ssos.h               "Simple SOS" common case SOSflow API wrapper,
 *                      useful for ports to additional languages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>

// The subset of data types supported by the SSOS-to-
// SOS interface.  When you pack data, you will be
// sending the void* cast address that points to the
// following memory footprints:
//   SSOS_TYPE_INT    = signed 32-bit integer
//   SSOS_TYPE_LONG   = signed 64-bit integer
//   SSOS_TYPE_DOUBLE = signed 64-bit float (IEEE 754)
//   SSOS_TYPE_STRING = null-terminated array of 1-byte char's
//
//   NOTE: Character constants in C like 'a' are encoded as
//         int32 types (C99:6.4.4.4, 883-886), though in
//         C++ they are encoded as char types (C++14x:2.13.3).
//         In practice this likely wont come up, but...
#define SSOS_TYPE_INT       1
#define SSOS_TYPE_LONG      2
#define SSOS_TYPE_DOUBLE    3
#define SSOS_TYPE_STRING    4

// These option keys can be used to set values inside of
// the various objects SOS uses to track application and
// publication metadata.  They are used as the first parameter
// of the SSOS_set_option(int key, const char *value) function:
#define SSOS_OPT_PROG_VERSION   1
#define SSOS_OPT_COMM_RANK      2 

// Reconnect tries during failed SOS_init() call.
#define SSOS_ATTEMPT_MAX    10

// Delay between connection attempts, useful to allow
// the SOS daemon to come online when processes have
// been launched concurrently with the daemon.
// (expressed in usec)
#define SSOS_ATTEMPT_DELAY  500000

// --------------------

typedef struct {
    void        *sos_context;
    char        *query_sql;
    uint64_t     query_guid;
    double       exec_duration;
    uint32_t     col_max;
    uint32_t     col_count;
    char       **col_names;
    uint32_t     row_max;
    uint32_t     row_count;
    char      ***data;
} SSOS_query_results;

#ifdef __cplusplus
extern "C" {
#endif

    void SSOS_init(const char *prog_name);
    void SSOS_is_online(int *addr_of_YN_int_flag);
    void SSOS_set_option(int option_key, const char *option_value);
    void SSOS_get_guid(void *addr_of_uint64);

    void SSOS_pack(const char *name, int pack_type, void *addr_of_value);
    void SSOS_announce(void);
    void SSOS_publish(void);
    void SSOS_finalize(void);

    void SSOS_query_exec(const char *sql, const char *target_host, int target_port);
    //
    void SSOS_request_pub_manifest(
        SSOS_query_results *manifest_var,
        int  *max_frame_overall_var,
        const char *pub_title_filter,
        const char *target_host,
        int  target_port);
    //
    void SSOS_cache_grab(
        const char *pub_filter,
        const char *val_filter,
        int   frame_head,
        int   frame_depth_limit,
        const char *target_host,
        int   target_port);
    //
    void SSOS_result_pool_size(int *addr_of_counter_int);
    void SSOS_result_claim(SSOS_query_results *results);
    void SSOS_result_destroy(SSOS_query_results *results);

    void SSOS_sense_trigger(
            const char *sense_handle,
            int payload_size, void *payload_data); 

#ifdef __cplusplus
}
#endif

#endif //SSOS_H
