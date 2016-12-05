
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

#define USAGE "./demo_app -i <iteration_size> -m <max_send_count> -p <pub_elem_count> [-j <jittertime.sec>]"


#include "sos.h"

#ifdef SOS_DEBUG
#undef SOS_DEBUG
#endif
#define SOS_DEBUG 1

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
    int    JITTER_ENABLED;
    double JITTER_INTERVAL;

    MPI_Init(&argc, &argv);

    /* Process command-line arguments */
    if ( argc < 5 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    MAX_SEND_COUNT  = -1;
    ITERATION_SIZE  = -1;
    PUB_ELEM_COUNT  = -1;
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
        } else if ( strcmp(argv[elem], "-p"  ) == 0) {
            PUB_ELEM_COUNT  = atoi(argv[next_elem]);
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
         || (PUB_ELEM_COUNT < 1)
         || (JITTER_INTERVAL < 0.0) )
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
    
    dlog(0, "Creating a pub...\n");

    pub = SOS_pub_create(my_sos, "demo", SOS_NATURE_CREATE_OUTPUT);
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
    var_int = 1234567890;
    snprintf(var_string, 100, "Hello, world!");

    var_double = 0.0;

    int pos = -1;
    for (i = 0; i < PUB_ELEM_COUNT; i++) {
        snprintf(elem_name, SOS_DEFAULT_STRING_LEN, "example_dbl_%d", i);
        pos = SOS_pack(pub, elem_name, SOS_VAL_TYPE_DOUBLE, &var_double);
        dlog(0, "   pub->data[%d]->guid == %" SOS_GUID_FMT "\n", pos, pub->data[pos]->guid);
        var_double += 0.0000001;
    }


    dlog(0, "Announcing\n");
    SOS_announce(pub);

    dlog(0, "Re-packing --> Publishing %d values for %d times per iteration:\n",
           PUB_ELEM_COUNT,
           ITERATION_SIZE);
           
    SOS_TIME( time_start );
    int mils = 0;
    int ones = 0;
 
    for (ones = 0; ones < ITERATION_SIZE; ones++) {
        for (i = 0; i < PUB_ELEM_COUNT; i++) {
            snprintf(elem_name, SOS_DEFAULT_STRING_LEN, "example_dbl_%d", i);
            SOS_pack(pub, elem_name, SOS_VAL_TYPE_DOUBLE, &var_double);
            var_double += 0.000001;
        }

        SOS_publish(pub);
    }

    /* Catch any stragglers. */
    //SOS_publish(pub);
    dlog(0, "  ... done.\n");

    dlog(0, "demo_app finished successfully!\n");
    SOS_finalize(my_sos);
    MPI_Finalize();
    return (EXIT_SUCCESS);
}
