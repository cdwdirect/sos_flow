#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <mpi.h>

#include "sos.h"
#include "sos_debug.h"

#define GET_TIME(__SOS_now)  { struct timeval t; gettimeofday(&t, NULL); __SOS_now = t.tv_sec + t.tv_usec/1000000.0; }

#define UNIQUE_VALUES     40
#define PACK_PUB_LOOPS    100

int size;
int rank;
SOS_pub *pub;

double ts_start;
double ts_finish;

double ts_TOTAL_start;
double ts_TOTAL_finish;
double tspan_TOTAL_in_app;
double max_tspan_TOTAL_in_app;
double min_tspan_TOTAL_in_app;
double avg_tspan_TOTAL_in_app;

double tspan_SOS_init;
double tspan_SOS_packing;
double tspan_SOS_announce;
double tspan_SOS_publish;

double max_tspan_SOS_init;
double max_tspan_SOS_announce;
double max_tspan_SOS_publish;
double max_tspan_SOS_packing;

double min_tspan_SOS_init;
double min_tspan_SOS_announce;
double min_tspan_SOS_publish;
double min_tspan_SOS_packing;

double avg_tspan_SOS_init;
double avg_tspan_SOS_announce;
double avg_tspan_SOS_publish;
double avg_tspan_SOS_packing;

double my_MISSING;
double min_MISSING;
double max_MISSING;
double avg_MISSING;

void run_test(int x);

int main(int argc, char **argv) {

    char pub_title[SOS_DEFAULT_STRING_LEN];
    char val_name[UNIQUE_VALUES][SOS_DEFAULT_STRING_LEN];
    double timenow;
    int i, n;

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    for (i = 0; i < UNIQUE_VALUES; i++) {
        memset(val_name[i], '\0', SOS_DEFAULT_STRING_LEN);
        sprintf(val_name[i], "rank(%d).name(%ld)", rank, random());
    }

    if (argc > 1 && ( strcmp(argv[1], "SHOW_HEADER" ) == 0)) {
        if (!rank) { printf("\"AGGREGATE\", \"MPI_SIZE\", \"SOS_init\", \"SOS_pack\", \"SOS_announce\", \"SOS_publish\", \"TOTAL_in_app\", \"NON_SOS_in_app\"\n"); }
        MPI_Finalize();
        exit(0);
    }


    /* Example variables. */
    char    *str_node_id  = getenv("HOSTNAME");
    char    *str_prog_ver = "1.0";
    char    *var_string   = "Hello, world!";
    int      var_int      = 10;
    double   var_double   = 88.8;

    MPI_Barrier(MPI_COMM_WORLD);

    SOS_init( &argc, &argv, SOS_ROLE_CLIENT );
    SOS_TIME(ts_TOTAL_start);
    SOS_TIME(ts_start);

    SOS_SET_WHOAMI(whoami, "main");
    SOS_TIME(ts_finish);
    tspan_SOS_init = ts_finish - ts_start;


    SOS_TIME(ts_start);
    pub = SOS_pub_create("demo");
    dlog(6, "[%s]: Manually configuring some pub metadata...\n", whoami);
    strcpy(pub->prog_ver, str_prog_ver);
    pub->meta.channel     = 1;
    pub->meta.nature      = SOS_NATURE_EXEC_WORK;
    /* Totally optional metadata, the defaults are usually right. */
    pub->meta.layer       = SOS_LAYER_APP;
    pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

    dlog(0, "[%s]: Packing a couple values...\n", whoami);
    i = SOS_pack(pub, "my_rank",     SOS_VAL_TYPE_INT,    (SOS_val) rank            );
    i = SOS_pack(pub, "example_int", SOS_VAL_TYPE_INT,    (SOS_val) var_int         );
    i = SOS_pack(pub, "example_str", SOS_VAL_TYPE_STRING, (SOS_val) var_string      );
    i = SOS_pack(pub, "example_dbl", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double      );
    SOS_TIME(ts_finish);
    tspan_SOS_packing = ts_finish - ts_start;

    for (i = 0; i < UNIQUE_VALUES; i++) {
        SOS_pack( pub, val_name[i], SOS_VAL_TYPE_LONG, (SOS_val) random());
    }

    dlog(0, "[%s]: Announcing the pub...\n", whoami);

    SOS_TIME(ts_start);
    SOS_announce(pub);
    SOS_TIME(ts_finish);
    tspan_SOS_announce = ts_finish - ts_start;

    dlog(0, "[%s]: Publishing the pub...\n", whoami);

    SOS_TIME(ts_start);
    SOS_publish(pub);
    SOS_TIME(ts_finish);
    tspan_SOS_publish = ts_finish - ts_start;

    for (n = 0; n < PACK_PUB_LOOPS; n++) {
        for (i = 0; i < UNIQUE_VALUES; i++) {
            SOS_pack( pub, val_name[i], SOS_VAL_TYPE_LONG, (SOS_val) random() );
        }
        SOS_publish( pub );
    }

    dlog(0, "[%s]: Done.\n", whoami);
    SOS_finalize();

    SOS_TIME(ts_TOTAL_finish);
    tspan_TOTAL_in_app = ts_TOTAL_finish - ts_TOTAL_start;

    my_MISSING = tspan_TOTAL_in_app - tspan_SOS_init - tspan_SOS_packing - tspan_SOS_announce - tspan_SOS_publish;
    
    /*
     *  Consolidate all of the values:
     */

    MPI_Reduce((void *) &tspan_TOTAL_in_app, (void *) &min_tspan_TOTAL_in_app, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_TOTAL_in_app, (void *) &max_tspan_TOTAL_in_app, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_TOTAL_in_app, (void *) &avg_tspan_TOTAL_in_app, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce((void *) &tspan_SOS_init, (void *) &min_tspan_SOS_init, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_init, (void *) &max_tspan_SOS_init, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_init, (void *) &avg_tspan_SOS_init, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce((void *) &tspan_SOS_packing, (void *) &min_tspan_SOS_packing, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_packing, (void *) &max_tspan_SOS_packing, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_packing, (void *) &avg_tspan_SOS_packing, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce((void *) &tspan_SOS_announce, (void *) &min_tspan_SOS_announce, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_announce, (void *) &max_tspan_SOS_announce, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_announce, (void *) &avg_tspan_SOS_announce, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce((void *) &tspan_SOS_publish, (void *) &min_tspan_SOS_publish, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_publish, (void *) &max_tspan_SOS_publish, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &tspan_SOS_publish, (void *) &avg_tspan_SOS_publish, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce((void *) &my_MISSING, (void *) &min_MISSING, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &my_MISSING, (void *) &max_MISSING, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce((void *) &my_MISSING, (void *) &avg_MISSING, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    avg_tspan_TOTAL_in_app   /= (double) size;
    avg_tspan_SOS_init       /= (double) size;
    avg_tspan_SOS_packing    /= (double) size;
    avg_tspan_SOS_announce   /= (double) size;
    avg_tspan_SOS_publish    /= (double) size;
    avg_MISSING              /= (double) size;

    if (!rank) {
        printf("\"MIN\", %d, %lf, %lf, %lf, %lf, %lf, %lf\n", size, min_tspan_SOS_init, min_tspan_SOS_packing, min_tspan_SOS_announce, min_tspan_SOS_publish, min_tspan_TOTAL_in_app, min_MISSING);
        printf("\"AVG\", %d, %lf, %lf, %lf, %lf, %lf, %lf\n", size, avg_tspan_SOS_init, avg_tspan_SOS_packing, avg_tspan_SOS_announce, avg_tspan_SOS_publish, avg_tspan_TOTAL_in_app, avg_MISSING);
        printf("\"MAX\", %d, %lf, %lf, %lf, %lf, %lf, %lf\n", size, max_tspan_SOS_init, max_tspan_SOS_packing, max_tspan_SOS_announce, max_tspan_SOS_publish, max_tspan_TOTAL_in_app, max_MISSING);
    }

    MPI_Finalize();
    return(0);
}

void run_test(int iterations) {
    int retval;
    int i;
    char letters[]="abcdefghijklmnopqrstuvwxyz";
    double before;
    double after;
    char msg[2048];
    MPI_Status status;

    GET_TIME(before);
    for (i = 0; i < iterations; i++) {
        if (rank == 0) {
            memset(msg, letters[i % 26], 2048); msg[2047] = '\0';
            MPI_Send((void *) msg, 2048, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
        } else { MPI_Recv(msg, 2048, MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status); }
    }; GET_TIME(after);
    if (rank == 0) printf("%10d, \t%1.8F\n", iterations, (double)(after - before));

    return;

};
