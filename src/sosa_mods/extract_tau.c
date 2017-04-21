
/*
 * extract_tau.c    (pull out tau values and forward them to independent sosa module)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define USAGE "./extract_tau -d <initial_delay_seconds>"

#include "sos.h"
#include "sosa.h"


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

    SOS_runtime *SOS = NULL;
    SOS_init(&argc, &argv, &SOS,
        SOS_ROLE_ANALYTICS, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (SOS == NULL) {
        fprintf(stderr, "ERROR: Could not initialize SOS.\n");
        exit (EXIT_FAILURE);
    }

    srandom(SOS->my_guid);
    sleep(initial_delay_seconds);


    /*
  ****   [ insert your analytics code here ]
     *
     */



    /*
  ****
     */

   SOS_finalize(SOS);

    return (EXIT_SUCCESS);
}
