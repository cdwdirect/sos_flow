
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

#define NUM_VALUES     20

#define USAGE "./demo_app -i <iteration_size> -m <max_send_count> [-j <jittertime.sec>]"


//#undef SOS_DEBUG
//#define SOS_DEBUG 1

#include "sos.h"
#include "sos_debug.h"

int main(int argc, char *argv[]) {

    SOS_runtime *my_sos;

    int i;
    int elem;
    int next_elem;
    char pub_title[SOS_DEFAULT_STRING_LEN];
    SOS_pub *pub;
    double time_now;
    double time_start;

    int    MAX_SEND_COUNT;
    int    ITERATION_SIZE;
    int    JITTER_ENABLED;
    double JITTER_INTERVAL;

    #if (SOSD_CLOUD_SYNC > 0)
    MPI_Init(&argc, &argv);
    #endif

    /* Process command-line arguments */
    if ( argc < 5 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    MAX_SEND_COUNT  = -1;
    ITERATION_SIZE  = -1;
    JITTER_ENABLED  = 0;
    JITTER_INTERVAL = 0.0;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-i"  ) == 0) {
            ITERATION_SIZE  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-m"  ) == 0) {
            MAX_SEND_COUNT  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-j"  ) == 0) {
            JITTER_INTERVAL = strtod(argv[next_elem], NULL);
            JITTER_ENABLED = 1;
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if ( (MAX_SEND_COUNT < 1)
         || (ITERATION_SIZE < 1)
         || (JITTER_INTERVAL < 0.0) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }


    /* Example variables. */
    char    *str_node_id  = getenv("HOSTNAME");
    char    *str_prog_ver = "1.0";
    char     var_string[100] = {0};
    int      var_int;
    double   var_double;
    
    snprintf(var_string, 100, "Hello, world!");

    my_sos = SOS_init( &argc, &argv, SOS_ROLE_CLIENT);
    SOS_SET_CONTEXT(my_sos, "demo_app.main");

    srandom(my_sos->my_guid);

    printf("demo_app starting...\n"); fflush(stdout);
    
    dlog(0, "Creating a pub...\n");

    pub = SOS_pub_create(my_sos, "demo");
    dlog(0, "  ... pub->guid  = %ld\n", pub->guid);

    dlog(0, "Manually configuring some pub metadata...\n");
    strcpy (pub->prog_ver, str_prog_ver);
    pub->meta.channel     = 1;
    pub->meta.nature      = SOS_NATURE_EXEC_WORK;
    pub->meta.layer       = SOS_LAYER_APP;
    pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub->meta.retain_hint = SOS_RETAIN_DEFAULT;


    dlog(0, "Packing a couple values...\n");
    var_double = 0.0;
    var_int = 0;

    SOS_pack(pub, "example_int", SOS_VAL_TYPE_INT,    (SOS_val) var_int         );
    SOS_pack(pub, "example_str", SOS_VAL_TYPE_STRING, (SOS_val) var_string      );

    var_double += 0.00001; SOS_pack(pub, "example_dbl_00", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_01", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_02", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_03", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_04", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_05", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_06", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_07", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_08", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_09", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    
    var_double += 0.00001; SOS_pack(pub, "example_dbl_10", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_11", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_12", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_13", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_14", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_15", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_16", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_17", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_18", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_19", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);

    dlog(0, "  ... Announcing\n");
    SOS_announce(pub);
    dlog(0, "  ... Publishing (initial)\n");
    SOS_publish(pub);

    dlog(0, "  ... Re-packing --> Publishing %d values for %d times per iteration:\n",
           NUM_VALUES,
           ITERATION_SIZE);
           
    SOS_TIME( time_start );
    int mils = 0;
    int ones = 0;
    while ((ones * NUM_VALUES) < MAX_SEND_COUNT) {
        ones += 1;
        if ((ones%ITERATION_SIZE) == 0) {
            SOS_TIME( time_now );
            dlog(0, "     ... [ %d calls to SOS_publish(%d vals) ][ %lf seconds @ %lf / value ][ total: %d values ]\n",
                   ITERATION_SIZE,
                   NUM_VALUES,
                   (time_now - time_start),
                   ((time_now - time_start) / (double) (NUM_VALUES * ITERATION_SIZE)),
                   (ones * NUM_VALUES));
            if (JITTER_ENABLED) {
                usleep((random() * 1000000) % (int)(JITTER_INTERVAL * 1000000));
            }
            SOS_TIME( time_start);
        }
        if (((ones * NUM_VALUES)%1000000) == 0) {
            dlog(0, "     ... 1,000,000 value milestone ---------\n");
        }

        var_double += 0.00001; SOS_pack(pub, "example_dbl_00", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_01", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_02", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_03", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_04", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_05", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_06", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_07", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_08", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_09", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);

        var_double += 0.00001; SOS_pack(pub, "example_dbl_10", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_11", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_12", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_13", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_14", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_15", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_16", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_17", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_18", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_19", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);

        if (ones % 2) {
            /* Publish every other iteration to force local snap-queue use. */
            SOS_publish(pub);
        }
    }
    /* Catch any stragglers. */
    SOS_publish(pub);
    dlog(0, "  ... done.\n");
    
    printf("demo_app finished successfully!\n"); fflush(stdout);

    SOS_finalize(my_sos);

    #if (SOSD_CLOUD_SYNC > 0)
    MPI_Finalize();
    #endif
    
    return (EXIT_SUCCESS);
}
