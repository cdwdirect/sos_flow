
/*
 * proc_app.c
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

#define USAGE "./proc_app -d <loop_delay_seconds>"

#define VERBOSE 1
#define log(...) if (VERBOSE) { printf( __VA_ARGS__ ); fflush(stdout); }

#include "sos.h"



typedef struct {
    char     name[10];
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
    uint64_t guest;
} proc_stat_t;

typedef struct {
    char     key[128];
    char     val[128];
} key_val_t;

int GLOBAL_sleep_delay;

int main(int argc, char *argv[]) {
    int   i;
    int   line;
    int   line_len;
    int   char_count;
    int   elem;
    int   next_elem;

    log("[MPI_init]\n");
    MPI_Init(&argc, &argv);

    /* Process command-line arguments */
    if ( argc < 3 )  { fprintf(stderr, "%s\n", USAGE); exit(1); }

    GLOBAL_sleep_delay = -1;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-d"  ) == 0) {
            GLOBAL_sleep_delay  = atoi(argv[next_elem]);
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if (GLOBAL_sleep_delay < 1) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    log("[SOS_init]\n");
    SOS_runtime *my_sos;
    my_sos = SOS_init( &argc, &argv, SOS_ROLE_RUNTIME_UTILITY, SOS_LAYER_ENVIRONMENT);

    srandom(my_sos->my_guid);

    SOS_pub *pub;    
    pub = SOS_pub_create(my_sos, "proc_app stat monitor", SOS_NATURE_SUPPORT_FLOW);

    char  val_handle[SOS_DEFAULT_STRING_LEN] = {0};
    char  proc_line[SOS_DEFAULT_STRING_LEN] = {0};
    FILE *proc_file;

    proc_stat_t stat;
    key_val_t   entry;

    while (getenv("SOS_SHUTDOWN") == NULL) {
        sleep(GLOBAL_sleep_delay);
        log("[wake]\n");

        log("  [open]  /proc/stat\n");
        proc_file = fopen("/proc/stat", "r");
        while( fgets(proc_line, SOS_DEFAULT_STRING_LEN, proc_file) ) {
            

        }
        log("  [publish]\n");
        SOS_publish(pub);
        log("[sleep]");
    }
    log("[SOS_finalize]\n");
    SOS_finalize(my_sos);
    log("[MPI_finalize]\b");
    MPI_Finalize();
    return (EXIT_SUCCESS);
}
