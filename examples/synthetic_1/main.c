/******************************************************************************
*     SOS_Flow example.
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "worker.h"
#include "util.h"
#include "main.h"

/* Global variables */
int commsize = 1;
int myrank = 0;
SOS_pub *example_pub = NULL; // sos.h is included by main.h
SOS_runtime * _runtime = NULL;

int main (int argc, char *argv[]) 
{
    /*
     * Initialize TAU and start a timer for the main function.
     */
    TAU_INIT(&argc, &argv);
    TAU_PROFILE_SET_NODE(0);
    TAU_PROFILE_TIMER(tautimer, __func__, __FILE__, TAU_USER);
    TAU_PROFILE_START(tautimer);

    /*
     * Initialize MPI. We don't require threaded support, but with threads
     * we can send the TAU data over SOS asynchronously.
     */
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

    /*
     * Initialize SOS. This will have been done in TAU, but in case we don't 
     * use TAU, we still want SOS action.
     */
    SOS_init_wrapper(&argc, &argv);

    /*
     * Run the worker code.
     */
    worker(argc, argv);

    /*
     * Finalize SOS and MPI
     */
    SOS_FINALIZE();
    MPI_Finalize();
    my_printf ("%s Done.\n", argv[0]);

    /*
     * Stop our main TAU timer. it probably will have been stopped in the MPI_Finalize call.
     */
    TAU_PROFILE_STOP(tautimer);
    return 0;
}

