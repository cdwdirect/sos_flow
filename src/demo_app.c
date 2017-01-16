
/*
 * demo_app.c
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

#define DEFAULT_MAX_SEND_COUNT 2400
#define DEFAULT_ITERATION_SIZE 25

#define USAGE "./demo_app -i <iteration_size> -m <max_send_count> -p <pub_elem_count> [-d <iter_delay_usec>]"


#include "sos.h"

/*
#ifdef SOS_DEBUG
#undef SOS_DEBUG
#endif
#define SOS_DEBUG 1
*/

#include "sos_debug.h"

int main(int argc, char *argv[]) {

    SOS_runtime *my_sos;

    int i;
    int elem;
    int next_elem;
    char pub_title[SOS_DEFAULT_STRING_LEN] = {0};
    char elem_name[SOS_DEFAULT_STRING_LEN] = {0};
    SOS_pub *pub;
    double time_now;
    double time_start;

    int    MAX_SEND_COUNT;
    int    ITERATION_SIZE;
    int    PUB_ELEM_COUNT;
    int    DELAY_ENABLED;
    double DELAY_IN_USEC;

    MPI_Init(&argc, &argv);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* Process command-line arguments */
    if ( argc < 5 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    MAX_SEND_COUNT  = -1;
    ITERATION_SIZE  = -1;
    PUB_ELEM_COUNT  = -1;
    DELAY_ENABLED   = 0;
    DELAY_IN_USEC   = 0;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-i"  ) == 0) {
            ITERATION_SIZE  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-m"  ) == 0) {
            MAX_SEND_COUNT  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-p"  ) == 0) {
            PUB_ELEM_COUNT  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-d"  ) == 0) {
            DELAY_IN_USEC = strtod(argv[next_elem], NULL);
            DELAY_ENABLED = 1;
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if ( (MAX_SEND_COUNT < 1)
         || (ITERATION_SIZE < 1)
         || (PUB_ELEM_COUNT < 1)
         || (DELAY_IN_USEC  < 0) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }


    /* Example variables. */
    char    *str_node_id  = getenv("HOSTNAME");
    char    *str_prog_ver = "1.0";
    char     var_string[100] = {0};
    int      var_int;
    double   var_double;
    
    my_sos = SOS_init( &argc, &argv, SOS_ROLE_CLIENT, SOS_LAYER_APP);
    SOS_SET_CONTEXT(my_sos, "demo_app.main");

    srandom(my_sos->my_guid);

    printf("demo_app starting...\n"); fflush(stdout);
    
    if (rank == 0) dlog(0, "Creating a pub...\n");

    pub = SOS_pub_create(my_sos, "demo", SOS_NATURE_CREATE_OUTPUT);
    if (rank == 0) dlog(0, "  ... pub->guid  = %" SOS_GUID_FMT "\n", pub->guid);

    if (rank == 0) dlog(0, "Manually configuring some pub metadata...\n");
    strcpy (pub->prog_ver, str_prog_ver);
    pub->meta.channel     = 1;
    pub->meta.nature      = SOS_NATURE_EXEC_WORK;
    pub->meta.layer       = SOS_LAYER_APP;
    pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub->meta.retain_hint = SOS_RETAIN_DEFAULT;


    if (rank == 0) dlog(0, "Packing a couple values...\n");
    var_int = 1234567890;
    snprintf(var_string, 100, "Hello, world!");

    SOS_pack(pub, "example_int", SOS_VAL_TYPE_INT,    &var_int         );
    SOS_pack(pub, "example_str", SOS_VAL_TYPE_STRING, var_string      );

    var_double = 0.0;

    int pos = -1;
    for (i = 0; i < PUB_ELEM_COUNT; i++) {
        snprintf(elem_name, SOS_DEFAULT_STRING_LEN, "example_dbl_%d", i);


        pos = SOS_pack(pub, elem_name, SOS_VAL_TYPE_DOUBLE, &var_double);


        dlog(0, "   pub->data[%d]->guid == %" SOS_GUID_FMT "\n", pos, pub->data[pos]->guid);
        var_double += 0.0000001;
    }


    if (rank == 0) dlog(0, "Announcing\n");
    SOS_announce(pub);

    if (rank == 0) dlog(0, "Re-packing --> Publishing %d values for %d times per iteration:\n",
           PUB_ELEM_COUNT,
           ITERATION_SIZE);
           
    SOS_TIME( time_start );
    int mils = 0;
    int ones = 0;
    while ((ones * PUB_ELEM_COUNT) < MAX_SEND_COUNT) {
        ones += 1;
        if ((ones%ITERATION_SIZE) == 0) {
            SOS_TIME( time_now );
            if (rank == 0) dlog(0, "     ... [ %d calls to SOS_publish(%d vals) ][ %lf seconds @ %lf / value ][ total: %d values ]\n",
                   ITERATION_SIZE,
                   PUB_ELEM_COUNT,
                   (time_now - time_start),
                   ((time_now - time_start) / (double) (PUB_ELEM_COUNT * ITERATION_SIZE)),
                   (ones * PUB_ELEM_COUNT));
            if (DELAY_ENABLED) {
                usleep(DELAY_IN_USEC);
            }
            SOS_TIME( time_start);
        }
        if (((ones * PUB_ELEM_COUNT)%1000000) == 0) {
            if (rank == 0) dlog(0, "     ... 1,000,000 value milestone ---------\n");
        }

        for (i = 0; i < PUB_ELEM_COUNT; i++) {
            snprintf(elem_name, SOS_DEFAULT_STRING_LEN, "example_dbl_%d", i);
            SOS_pack(pub, elem_name, SOS_VAL_TYPE_DOUBLE, &var_double);
            var_double += 0.000001;
        }

        if (ones % 2) {
            /* Publish every other iteration to force local snap-queue use. */
            SOS_publish(pub);
        }
        //SOS_publish(pub);
    }

    dlog(0, "  ... done.\n");

    if (rank == 0) dlog(0, "demo_app finished successfully!\n");
    SOS_finalize(my_sos);
    MPI_Finalize();
    return (EXIT_SUCCESS);
}
