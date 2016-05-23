/*
 *   sosa.c   Library functions for writing SOS analytics modules.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <mpi.h>

#include "sos.h"
#include "sosd.h"
#include "sosa.h"
#include "sos_types.h"
#include "sos_debug.h"

SOSA_runtime SOSA;


SOS_runtime* SOSA_init(int *argc, char ***argv, int unique_color) {
    SOSA.sos_context = (SOS_runtime *) malloc(sizeof(SOS_runtime));

    SOSA.sos_context->role = SOS_ROLE_ANALYTICS;
    SOSA.sos_context->status = SOS_STATUS_RUNNING;
    SOSA.sos_context->config.argc = *argc;
    SOSA.sos_context->config.argv = *argv;

    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &SOSA.sos_context->config.comm_support);

    int universe_size = -1;
    int universe_rank = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &universe_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &universe_rank);
    MPI_Comm_split(MPI_COMM_WORLD, (unique_color + SOS_ROLE_ANALYTICS), universe_rank, &SOSA.comm);

    MPI_Comm_size(SOSA.comm, &SOSA.sos_context->config.comm_size);
    MPI_Comm_rank(SOSA.comm, &SOSA.sos_context->config.comm_rank);
    
    
    return NULL;
}

void SOSA_lock_db(void) {
    return;
}

void SOSA_unlock_db(void) {
    return;
}



void SOSA_run_query(char *sql_string, SOS_buffer *result) {
    return;
}

void SOSA_finalize(void) {
    return;
}

