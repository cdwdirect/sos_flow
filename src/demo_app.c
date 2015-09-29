
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
    int i, thread_support;
    char pub_title[SOS_DEFAULT_STRING_LEN];
    SOS_pub *pub;
    SOS_pub *pub2;
    SOS_sub *sub;
    pthread_t repub_t;
    double timenow;

    /* Example variables. */
    char    *str_node_id  = getenv("HOSTNAME");
    char    *str_prog_ver = "1.0";
    char    *var_string   = "Hello, world!";
    int      var_int      = 10;
    double   var_double   = 88.8;
    
    SOS_init( &argc, &argv, SOS_ROLE_CLIENT );
    SOS_SET_WHOAMI(whoami, "main");

    dlog(0, "[%s]: Creating two new pubs...\n", whoami);
    pub = SOS_new_pub("demo");
    pub2 = SOS_new_pub("demo2");

    dlog(0, "[%s]:   ... pub->guid  = %ld\n", whoami, pub->guid);
    dlog(0, "[%s]:   ... pub2->guid = %ld\n", whoami, pub2->guid);

    dlog(6, "[%s]: Manually configuring some pub metadata...\n", whoami);
    pub->prog_ver         = str_prog_ver;
    pub->meta.channel     = 1;
    pub->meta.nature      = SOS_NATURE_EXEC_WORK;
    /* Totally optional metadata, the defaults are usually right. */
    pub->meta.layer       = SOS_LAYER_APP;
    pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

    dlog(6, "[%s]: Manually configuring some pub metadata...\n", whoami);
    pub2->prog_ver         = str_prog_ver;
    pub2->meta.channel     = 2;
    pub2->meta.nature      = SOS_NATURE_CREATE_VIZ;
    /* Totally optional metadata, the defaults are usually right. */
    pub2->meta.layer       = SOS_LAYER_APP;
    pub2->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub2->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub2->meta.retain_hint = SOS_RETAIN_DEFAULT;

    dlog(0, "[%s]: Packing a couple values...\n", whoami);
    i = SOS_pack(pub, "example_int", SOS_VAL_TYPE_INT,    (SOS_val) var_int         );
    i = SOS_pack(pub, "example_str", SOS_VAL_TYPE_STRING, (SOS_val) var_string      );
    i = SOS_pack(pub, "example_dbl", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double      );
    
    dlog(0, "[%s]: Announcing the pub...\n", whoami);
    SOS_announce(pub);

    dlog(0, "[%s]: Publishing the pub...\n", whoami);
    SOS_publish(pub);

    dlog(0, "[%s]: Re-packing the last value...\n", whoami);
    var_double = 99.9;
    SOS_repack(pub, i, (SOS_val) var_double);

    var_double = 77.7;

    i = SOS_pack(pub2, "example_int", SOS_VAL_TYPE_INT,    (SOS_val) var_int         );
    i = SOS_pack(pub2, "example_str", SOS_VAL_TYPE_STRING, (SOS_val) var_string      );
    i = SOS_pack(pub2, "example_dbl", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double      );

    dlog(0, "[%s]: Publishing pub2 w/out announcing (should automatically announce)\n", whoami);
    SOS_publish(pub2);

    dlog(0, "[%s]: Re-Publishing the first pub w/one updated value.\n", whoami);
    SOS_publish(pub);

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
