
/*
 * demo_app.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


#define MAX_SEND_COUNT 10000
#define ITERATION_SIZE 1000
#define NUM_VALUES     20

#undef SOS_DEBUG
#define SOS_DEBUG 0

#include "sos.h"

int main(int argc, char *argv[]) {
    int i;
    char pub_title[SOS_DEFAULT_STRING_LEN];
    SOS_pub *pub;
    SOS_pub *pub2;
    SOS_sub *sub;
    pthread_t repub_t;
    double time_now;
    double time_start;

    /* Example variables. */
    char    *str_node_id  = getenv("HOSTNAME");
    char    *str_prog_ver = "1.0";
    char    *var_string   = "Hello, world!";
    int      var_int;
    double   var_double;
    
    SOS_init( &argc, &argv, SOS_ROLE_CLIENT );
    SOS_SET_WHOAMI(whoami, "demo_app.main");

    printf("[%s]: Creating a pub...\n", whoami);
    pub = SOS_pub_create("demo");
    printf("[%s]:   ... pub->guid  = %ld\n", whoami, pub->guid);

    printf("[%s]: Manually configuring some pub metadata...\n", whoami);
    pub->prog_ver         = str_prog_ver;
    pub->meta.channel     = 1;
    pub->meta.nature      = SOS_NATURE_EXEC_WORK;
    pub->meta.layer       = SOS_LAYER_APP;
    pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub->meta.retain_hint = SOS_RETAIN_DEFAULT;


    printf("[%s]: Packing a couple values...\n", whoami);
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

    /*
    var_double += 0.00001; SOS_pack(pub, "example_dbl_20", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_21", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_22", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_23", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_24", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_25", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_26", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_27", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_28", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_29", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    
    var_double += 0.00001; SOS_pack(pub, "example_dbl_30", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_31", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_32", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_33", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_34", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_35", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_36", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_37", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_38", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    var_double += 0.00001; SOS_pack(pub, "example_dbl_39", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
    */

    printf("[%s]:   ... Announcing\n", whoami);
    SOS_announce(pub);
    printf("[%s]:   ... Publishing (initial)\n", whoami);
    SOS_publish(pub);

    printf("[%s]:   ... Re-packing --> Publishing %d values for %d times per iteration:\n",
           whoami,
           NUM_VALUES,
           ITERATION_SIZE);
           
    SOS_TIME( time_start );
    int mils = 0;
    int ones = 0;
    while (((mils * 1000000) + ones) < MAX_SEND_COUNT) {
        ones += 1;
        if ((ones%ITERATION_SIZE) == 0) {
            SOS_TIME( time_now );
            printf("[%s]:      ... [%dmil] + %d   (%lf seconds total @ %lf / value)\n",
                   whoami,
                   mils,
                   ones,
                   (time_now - time_start),
                   ((time_now - time_start) / (double) (NUM_VALUES * ITERATION_SIZE)));
            usleep(1000);
            SOS_TIME( time_start);
        }
        if (ones > 1000000) {
            mils += 1;
            ones = 0;
            printf("[%s]:      ... 1,000,000 value milestone ---------\n", whoami);
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

        /*
        var_double += 0.00001; SOS_pack(pub, "example_dbl_20", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_21", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_22", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_23", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_24", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_25", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_26", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_27", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_28", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_29", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);

        var_double += 0.00001; SOS_pack(pub, "example_dbl_30", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_31", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_32", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_33", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_34", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_35", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_36", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_37", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_38", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        var_double += 0.00001; SOS_pack(pub, "example_dbl_39", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double);
        */

        SOS_publish(pub);
    }
    printf("[%s]:   ... done.\n", whoami);
    printf("[%s]: Shutting down!\n", whoami);
    SOS_finalize();
    
    return (EXIT_SUCCESS);
}
