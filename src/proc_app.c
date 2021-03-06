/*
 * proc_app.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#ifdef SOSD_CLOUD_SYNC_WITH_MPI
  #include <mpi.h>
  #define IF_MPI(X) {if (SOSD_CLOUD_SYNC_WITH_MPI) {X} }
#else
  #define SOSD_CLOUD_SYNC_WITH_MPI 0
  #define IF_MPI(X)   
  #define MPI_COMM_WORLD -1
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
} proc_cpustat_t;

typedef struct {
    char     key[128];
    char     val[128];
} key_val_t;

int GLOBAL_sleep_delay;

int main(int argc, char *argv[]) {
    int    i;
    int    line;
    size_t line_len;
    int    rc;
    int    char_count;
    int    elem;
    int    next_elem;

    IF_MPI( log("[MPI_init]\n"); );
    IF_MPI( MPI_Init(&argc, &argv); );

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
    SOS_runtime *my_sos = NULL;
    SOS_init( &my_sos, SOS_ROLE_RUNTIME_UTILITY, SOS_RECEIVES_NO_FEEDBACK, NULL);

    srandom(my_sos->my_guid);

    SOS_pub *pub;    
    SOS_pub_init(my_sos, &pub, "proc_app stat monitor", SOS_NATURE_SUPPORT_FLOW);

    char  val_handle[SOS_DEFAULT_STRING_LEN] = {0};
    char *proc_line = NULL;
    FILE *proc_file;

    proc_cpustat_t cpustat;
    key_val_t      entry;

    int iteration   = 0;
    int line_number = 0;
    while (!SOS_str_opt_is_enabled(getenv("SOS_SHUTDOWN"))) {
        sleep(GLOBAL_sleep_delay);
        log("[wake]\n");

        log("  [open]  /proc/stat\n");
        proc_file = fopen("/proc/stat", "r");

        line_number = 0;
        while((rc = getline(&proc_line, &line_len, proc_file)) != -1) {

            memset(&cpustat, 0, sizeof(cpustat));

            if (strncmp(proc_line, "cpu", 3) == 0) {
                sscanf(proc_line, "%9s %" PRIu64" %" PRIu64" %" PRIu64" %" PRIu64" %" PRIu64" %" PRIu64" %" PRIu64" %" PRIu64 " %" PRIu64,
                       cpustat.name,
                       &cpustat.user,
                       &cpustat.nice,
                       &cpustat.system,
                       &cpustat.idle,
                       &cpustat.iowait,
                       &cpustat.irq,
                       &cpustat.softirq,
                       &cpustat.steal,
                       &cpustat.guest);

                uint64_t jiffs_total   = 0;
                double   avg_idle_pct  = 0;

                jiffs_total = (cpustat.user + cpustat.nice + cpustat.system + cpustat.idle
                               + cpustat.iowait + cpustat.irq + cpustat.softirq
                               + cpustat.steal + cpustat.guest);

                SOS_val idle_pct;
                idle_pct.d_val = (cpustat.idle * 100) / jiffs_total;

                if (strlen(cpustat.name) == 3) {
                    // Aggregate case:
                    SOS_pack(pub, "cpu:idle_pct", SOS_VAL_TYPE_DOUBLE, (void *) &idle_pct);
                } else {
                    // Per-processor case:
                    char cpuname[24] = {0};
                    snprintf(cpuname, 24, "%s:idle_pct", cpustat.name);
                    SOS_pack(pub, cpuname, SOS_VAL_TYPE_DOUBLE, (void *) &idle_pct);
                }

                free(proc_line);
            }//if:cpu*
            
            proc_line = NULL;
            line_number++;
        }//while:getline
        iteration++;
        log("    ----------\n");
        fclose(proc_file);
        log("  [publish]\n");
        SOS_publish(pub);
        log("[sleep]");
    }//while:running
    log("[SOS_finalize]\n");
    SOS_finalize(my_sos);
    IF_MPI( log("[MPI_finalize]\b"); );
    IF_MPI( MPI_Finalize(); );
    return (EXIT_SUCCESS);
}
