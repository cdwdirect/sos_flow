
/*
 * demo_app.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "sos.h"
#include "sos_debug.h"

int demo_run;


void* repub_thread(void *arg) {
    while (demo_run) { ; }   
    return NULL;
}


int main(int argc, char *argv[]) {
    int i, n, k, thread_support;
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

    dlog(0, "[%s]: Creating a pub...\n", whoami);
    pub = SOS_pub_create("demo");
    dlog(0, "[%s]:   ... pub->guid  = %ld\n", whoami, pub->guid);

    dlog(6, "[%s]: Manually configuring some pub metadata...\n", whoami);
    pub->prog_ver         = str_prog_ver;
    pub->meta.channel     = 1;
    pub->meta.nature      = SOS_NATURE_EXEC_WORK;
    pub->meta.layer       = SOS_LAYER_APP;
    pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub->meta.retain_hint = SOS_RETAIN_DEFAULT;


    dlog(0, "[%s]: Packing a couple values...\n", whoami);
    var_double = 0.0;
    var_int = 0;

    i = SOS_pack(pub, "example_int", SOS_VAL_TYPE_INT,    (SOS_val) var_int         );
    i = SOS_pack(pub, "example_str", SOS_VAL_TYPE_STRING, (SOS_val) var_string      );
    i = SOS_pack(pub, "example_dbl", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double      );
   
    dlog(0, "[%s]: Announcing the pub...\n", whoami);
    SOS_announce(pub);

    dlog(0, "[%s]: Publishing the pub...\n", whoami);
    SOS_publish(pub);

    SOS_TIME( time_start );

    n = 0;
    k = 0;
    while (1) {
        n += 1;
        if ((n%100) == 0) {
            SOS_TIME( time_now );
            printf("[%d].%d   (%lf seconds)\n", k, n, (time_now - time_start));
            SOS_TIME( time_start);
        }
        if (n > 1000000) {
            k += 1;
            n == 0;
            printf("[---resetting---]\n");
        }
        usleep(2500);

        var_double += 0.00001;
        SOS_repack(pub, i, (SOS_val) var_double);
        SOS_publish(pub);
    }

    /*  LOOP VERSION...
     *
    dlog(0, "[%s]: Re-packing the last value...\n", whoami);
    for (n = 0; n < 1000; n++) {
        if (n%50 == 0) {
            SOS_TIME( time_now );
            dlog(0, "[%s]: %d iterations of SOS_repack() --> SOS_publish()...    (%lf seconds elapsed)\n", whoami, n, (time_now - time_start) );
        }
        var_double += 0.001;
        SOS_repack(pub, i, (SOS_val) var_double);
        SOS_publish(pub);
    }
    SOS_TIME( time_now );
    dlog(0, "[%s]: Total time for 1,000 immediate SOS_repack() --> SOS_publish() calls: %lf seconds\n", whoami, (time_now - time_start));
    */

    /*
     *  Skipping the threaded part for now.
     *
    demo_run = 1;
    pthread_create(&repub_t, NULL, repub_thread, (void*)pub);    
    pthread_join(repub_t, NULL);
     */
    
    dlog(0, "[%s]: Shutting down!\n", whoami);
    SOS_finalize();
    
    return (EXIT_SUCCESS);
}
