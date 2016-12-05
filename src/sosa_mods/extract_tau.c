
/*
 * extract_tau.c    (pull out tau values and forward them to independent sosa module)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#if (SOSD_CLOUD_SYNC > 0)
#include <mpi.h>
#endif

#define USAGE "./extract_tau -d <initial_delay_seconds>"

#include "sos.h"
#include "sosd.h"
#include "sosa.h"
#include "sos_debug.h"


int main(int argc, char *argv[]) {


    /* Process command-line arguments */
    if ( argc < 3 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    int initial_delay_seconds = 0;
    int elem, next_elem = 0;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-d"  ) == 0) {
            initial_delay_seconds  = atoi(argv[next_elem]);
        } else {
            fprintf(stderr, "ERROR: Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }
        elem = next_elem + 1;
    }


    SOSA.sos_context = SOSA_init_for_mpi( &argc, &argv, 800);
    SOS_SET_CONTEXT(SOSA.sos_context, "main");
    srandom(SOS->my_guid);
    dlog(1, "Initialization complete.\n");

    sleep(initial_delay_seconds);


    /*
  ****   [ insert your analytics code here ]
     *
     */



    /*
  ****
     */

    dlog(0, "Finished successfully!\n");
    SOSA_finalize();
    MPI_Finalize();

    return (EXIT_SUCCESS);
}
