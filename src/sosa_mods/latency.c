
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

// NOTE:SOSA_MODULE_COLOR ....... should be unique among active sosa modules.
#define SOSA_MODULE_COLOR   808

#include "sos.h"
#include "sosd.h"
#include "sosa.h"
#include "sos_debug.h"


char *file_path;


int main(int argc, char *argv[]) {


    /* Process command-line arguments */
    if ( argc < 3 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    int initial_delay_seconds = 0;
    int elem, next_elem = 0;

    file_path = NULL;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-d"  ) == 0) {
            initial_delay_seconds  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-f" ) == 0) {
            file_path = argv[next_elem];
        } else {
            fprintf(stderr, "ERROR: Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }
        elem = next_elem + 1;
    }

    SOSA.sos_context = SOSA_init_for_mpi( &argc, &argv, SOSA_MODULE_COLOR);
    SOS_SET_CONTEXT(SOSA.sos_context, "latency:main()");
    srandom(SOS->my_guid);
    dlog(0, "Initialization complete.\n");
    dlog(0, "Waiting for %d seconds...\n", initial_delay_seconds);
    sleep(initial_delay_seconds);


    /*
  ****   [ insert your analytics code here ]
     *
     */

    SOSA_results *results;
    SOSA_results_init(SOS, &results);

    char *query    = "SELECT max(rowid), time_pack, time_recv FROM tblvals;";
    int   count    = 0;

    FILE *fptr = NULL;

    if (file_path == NULL) {
      fptr = fopen("sosa_latency.csv", "a");
    } else {
      fptr = fopen(file_path, "a");
    }

    do {
        SOSA_results_wipe(results);
        dlog(0, "Submitting query request #%d...\n", count);
        SOSA_exec_query(query, results);

        SOSA_results_output_to(fptr, results, "latency", SOSA_OUTPUT_DEFAULT);

        usleep(5000000); //1,000,000 = 1 sec.
    } while(atoi(results->data[0][0]) < 14000000);

    fclose(fptr);

    SOSA_results_destroy(results);
    
    /*
  ****
     */

    dlog(0, "Finalizing analytics module...  (will block until SOS runtime is stopped)\n");
    SOSA_finalize();
    MPI_Finalize();
    dlog(0, "    ... done.\n");
    return (EXIT_SUCCESS);
}
