
/*
 * sosa_latency.c    (demo of analytics: calculate various latency-related figures)
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

#define USAGE "./latency -d <initial_delay_seconds>"

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


    SOSA.sos_context = SOSA_init( &argc, &argv, 800);
    SOS_SET_CONTEXT(SOSA.sos_context, "main");
    srandom(SOS->my_guid);
    dlog(1, "Initialization complete.\n");

    sleep(initial_delay_seconds);


    /*
  ****   [ insert your analytics code here ]
     *
     */

    SOSA_results *results;
    SOSA_results_init(SOS, &results);

    char *query    = "SELECT max(rowid), time_pack, time_recv FROM tblvals;";
    int   count    = 0;

    for (count = 0; count < 10; count++) {
        SOSA_results_wipe(results);
        SOSA_exec_query(query, results);
        SOSA_results_output_to(stdout, results, 0);
        usleep(1000000); //1,000,000 = 1 sec.
    }

    SOSA_results_destroy(results);
    
    /*
  ****
     */

    dlog(0, "Finished successfully!\n");
    SOSA_finalize();
    MPI_Finalize();

    return (EXIT_SUCCESS);
}
