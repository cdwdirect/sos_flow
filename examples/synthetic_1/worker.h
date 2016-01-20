#pragma once

#include "mpi.h"

/* Global variables */
extern int commsize;
extern int myrank;

/* The main entry point for the worker */
int worker(int argc, char* argv[]);

/* The main computation for the worker */
int compute (int iteration);

/* make sure only rank 0 prints output */
#define my_printf(...) if (myrank == 0) { printf(__VA_ARGS__); fflush(stdout); }

static inline int get_left_neighbor() {
    if (myrank == 0) { return commsize-1; }
    return myrank-1;
}

static inline int get_right_neighbor() {
    if (myrank == (commsize-1)) { return 0; }
    return myrank+1;
}
