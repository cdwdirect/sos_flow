#ifndef SOSA_H
#define SOSA_H

#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <mpi.h>

#include "sos.h"
#include "sosd.h"
#include "sos_types.h"


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
    void SOSA_run_query(char *sql_string, SOS_buffer *result);

    void SOSA_finalize(void);

#ifdef __cplusplus
}
#endif



#endif
