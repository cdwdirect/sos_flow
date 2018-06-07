#ifndef SOSA_H
#define SOSA_H

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_target.h"

#define SOSA_DEFAULT_RESULT_ROW_MAX 16*1024
#define SOSA_DEFAULT_RESULT_COL_MAX 5 


typedef enum {
    SOSA_OUTPUT_DEFAULT    = 0,          /* CSV w/no header */
    SOSA_OUTPUT_JSON       = (1 << 0),
    SOSA_OUTPUT_W_HEADER   = (1 << 1)   /* Ignored by _JSON */
} SOSA_output_options;


typedef struct {
    SOS_runtime *sos_context;
    char        *query_sql;
    SOS_guid     query_guid;
    double       exec_duration;
    int          col_max;
    int          col_count;
    char       **col_names;
    int          row_max;
    int          row_count;
    char      ***data;
} SOSA_results;


/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif


    // SQL: Submit an SQL query to a SOS daemon over the socket:
    SOS_guid SOSA_exec_query(SOS_runtime *sos_context,
            char *sql_string, char *target_host, int target_port);

    // CACHE: Gather current values belonging to matching pub and value names:
    //      frame_head:
    //          Start at this frame and go backward  (-1 == LATEST)
    //      frame_depth_limit:
    //          How many frames may be included (-1 || 0 == NO LIMIT)
    //
    SOS_guid SOSA_cache_grab(SOS_runtime *sos_context,
            char *pub_filter_regex, char *val_filter_regex,
            int frame_head, int frame_depth_limit,
            char *target_host, int target_port);
    //
    void SOSA_cache_to_results(SOS_runtime *sos_context, SOSA_results *results,
            char *pub_filter_regex, char *val_filter_regex,
            int frame_head, int frame_depth_limit);

    // Utilities for working with result sets:
    void SOSA_results_init(SOS_runtime *sos_context,
            SOSA_results **results_object_ptraddr);
    void SOSA_results_label(SOSA_results *results, SOS_guid guid, const char *sql);
    void SOSA_results_grow_to(SOSA_results *results, int new_col_max, int new_row_max);
    void SOSA_results_put_name(SOSA_results *results, int col, const char *name);
    void SOSA_results_put(SOSA_results *results, int col, int row, const char *value);
    void SOSA_results_output_to(FILE *file,
            SOSA_results *results, char *title, int options);
    void SOSA_results_to_buffer(SOS_buffer *buffer, SOSA_results *results);
    void SOSA_results_from_buffer(SOSA_results *results, SOS_buffer *buffer);
    void SOSA_results_wipe(SOSA_results *results_object);
    void SOSA_results_destroy(SOSA_results *results_object);

    //TODO: Quickly "tack on" one set of results to another, assume identical schemas: 
    //void SOSA_results_append(SOSA_results *to_set, SOSA_results *from_set);
    //TODO: Ensure that column names match as results are appended, adding columns as needed:
    //void SOSA_results_merge(SOSA_results *to_set, SOSA_results *from_set);

    // Shortcut function to send to 'our default target':
    void SOSA_send_to_target_db(SOS_buffer *msg, SOS_buffer *reply);

    //DEPRECATED:
    //void SOSA_guid_request(SOS_runtime *sos_context, SOS_uid *uid);


#ifdef __cplusplus
}
#endif


#endif
//SOSA_H
