#pragma once

#include "mpi.h"
#include "adios.h"
#include "util.h"

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

static inline void do_neighbor_exchange(void) {
    int outdata[100] = {1};
    int indata[100] = {0};
    int thistag = 1;
    if (commsize < 2) return;
    MPI_Status status;
    if (myrank % 2 == 0) {
        // send to left neighbor
        MPI_Send(outdata,              /* message buffer */
                 100,               /* one data item */
                 MPI_INT,           /* data item is an integer */
                 get_left_neighbor(),              /* destination process rank */
                 thistag,           /* user chosen message tag */
                 MPI_COMM_WORLD);   /* default communicator */
        int thistag = 2;
        // receive from right neighbor
        MPI_Recv(indata,            /* message buffer */
                 100,                 /* one data item */
                 MPI_INT,        /* of type double real */
                 get_right_neighbor(),    /* receive from any sender */
                 thistag,       /* any type of message */
                 MPI_COMM_WORLD,    /* default communicator */
                 &status);          /* info about the received message */
    } else {
        // receive from right neighbor
        MPI_Recv(indata,            /* message buffer */
                 100,                 /* one data item */
                 MPI_INT,        /* of type double real */
                 get_right_neighbor(),    /* receive from any sender */
                 thistag,       /* any type of message */
                 MPI_COMM_WORLD,    /* default communicator */
                 &status);          /* info about the received message */
        int thistag = 2;
        // send to left neighbor
        MPI_Send(outdata,              /* message buffer */
                 100,               /* one data item */
                 MPI_INT,           /* data item is an integer */
                 get_left_neighbor(),              /* destination process rank */
                 thistag,           /* user chosen message tag */
                 MPI_COMM_WORLD);   /* default communicator */
    }

}


