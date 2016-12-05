#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define USAGE "./end_stats <memory_bytes>"

void showstats(void) {
    char statpath[1024] = {0};
    char line[1024] = {0};
    int  line_count = 0;
    FILE *statfile;

    snprintf(statpath, 1024, "/proc/%d/status", getpid());
    statfile = fopen(statpath, "r");

    if (statfile < 0) {
        printf("ERROR: Could not open status file.  (%s)\n", statpath);
        return;
    }
    
    while (!feof(statfile)) {
        if (fgets(line, sizeof(line), statfile)) {
            printf("%d: %s", line_count++, line);
        }
    }

    return;
}

int main(int argc, char **argv) {
    long a;
    int i;

    if (argc != 2) {
        printf("USAGE: %s\n", USAGE);
        exit(EXIT_FAILURE);
    }

    int MEMMAX = atoi(argv[1]);
    
    a = sysconf(_SC_ATEXIT_MAX);
    printf("Setting exit function...  (ATEXIT_MAX = %ld)\n", a);
    
    i = atexit(showstats);
    if (i != 0) {
        fprintf(stderr, "ERROR: Cannot set exit function!\n");
        exit(EXIT_FAILURE);
    }

    printf("Allocating %d bytes...\n", MEMMAX);

    char *mem;
    mem = (char *) malloc(MEMMAX * sizeof(char));

    printf("Filling %d bytes with 'x'...\n", MEMMAX);
    i = 0;
    for (i = 0; i < MEMMAX; i++) {
        mem[i] = 'x';
    }
    printf("Done.\n");

    printf("Freeing memory.\n");
    free(mem);

    printf("Exiting.\n"); fflush(stdout);
    
    exit(EXIT_SUCCESS);
}
