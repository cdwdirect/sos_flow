/******************************************************************************
*     SOS_Flow example.
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "worker.h"
#include "util.h"

/* Global variables */
int commsize = 1;
int myrank = 0;

int main (int argc, char *argv[]) 
{
    TAU_INIT(&argc, &argv);
    TAU_PROFILE_SET_NODE(0);
    TAU_PROFILE_TIMER(tautimer, __func__, __FILE__, TAU_USER);
    TAU_PROFILE_START(tautimer);

    int rc = MPI_SUCCESS;
    int provided = 0;
    rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (rc != MPI_SUCCESS) {
        char *errorstring;
        int length = 0;
        MPI_Error_string(rc, errorstring, &length);
        printf("Error: MPI_Init failed, rc = %d\n%s\n", rc, errorstring);
        exit(1);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
    MPI_Comm_size(MPI_COMM_WORLD, &commsize);
    //my_printf("MPI_Init_thread: provided = %d, MPI_THREAD_MULTIPLE=%d\n", provided, MPI_THREAD_MULTIPLE);
    my_printf("%s Running with commsize %d\n", argv[0], commsize);

    worker(argc, argv);

    MPI_Finalize();
    my_printf ("%s Done.\n", argv[0]);

    TAU_PROFILE_STOP(tautimer);
    return 0;
}

