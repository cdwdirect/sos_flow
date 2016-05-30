#ifndef SOSA_H
#define SOSA_H

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <mpi.h>

#include "sos.h"
#include "sosd.h"
#include "sos_types.h"


typedef enum {
    SOSA_OUTPUT_DEFAULT    = 0,          /* CSV w/no header */
    SOSA_OUTPUT_JSON       = (1 << 0),
    SOSA_OUTPUT_W_HEADER   = (1 << 1)   /* Ignored by _JSON */
} SOSA_output_options;




typedef struct {
    SOS_runtime *sos_context;
    int          col_count;
    char       **col_names;
    int          row_count;
    char      ***row_field;
} SOSA_results;


typedef struct {
    SOS_runtime *sos_context;
    MPI_Comm     comm;
    int         *analytics_locales;
    int          world_rank;
    int          world_size;
    int         *world_roles;
    char        *world_hosts;
    int          world_db_target_rank;
} SOSA_runtime;

extern SOSA_runtime SOSA;

/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

    SOS_runtime* SOSA_init(int *argc, char ***argv, int unique_color);

    void SOSA_guid_request(SOS_uid *uid);
    void SOSA_exec_query(char *sql_string, SOSA_results *result_object);

    void SOSA_results_init(SOS_runtime *sos_context, SOSA_results **results_object_ptraddr);
    void SOSA_results_output_to(FILE *file, SOSA_results *results, int options);
    void SOSA_results_to_buffer(SOS_buffer *buffer, SOSA_results *results);
    void SOSA_results_from_buffer(SOSA_results *results, SOS_buffer *buffer);
    void SOSA_results_wipe(SOSA_results *results_object);
    void SOSA_results_destroy(SOSA_results *results_object);

    void SOSA_finalize(void);


/* TREAT LIKE A PRIVATE FUNCTION: Make sure you know what you're
 *  doing... this is not to be used a part of the API for a reason, as
 *  specific handlers have to be written for each message type, and the
 *  messages have to be hand-packed in the correct way. If you're
 *  having to use this for some common functionality, perhaps it would
 *  be better to extend and enrich the API to provide much safer
 *  function calls. It exists to consolidate all MPI calls in one place
 *  for use by such functions.
 */
void SOSA_send_to_target_db(SOS_buffer *message, SOS_buffer *reply);





#ifdef __cplusplus
}
#endif



#endif
