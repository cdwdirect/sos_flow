#include "worker.h"
#include "stdio.h"
#include "stdlib.h"

/* 
 * Worker A is the first application in the workflow.
 * It does some "computation" and communication for N
 * iterations, and it produces "output" that is "input" to worker_b.
 */

int iterations = 1;

void validate_input(int argc, char* argv[]) {
    if (argc < 2) {
        my_printf("Usage: %s <num iterations>\n", argv[0]);
        exit(1);
    }
    if (commsize < 2) {
        my_printf("%s requires at least 2 processes.\n", argv[0]);
        exit(1);
    }
    iterations = atoi(argv[1]);
}

void do_neighbor_exchange(void) {
    int outdata[100] = {1};
    int indata[100] = {0};
    int thistag = 1;
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

int worker(int argc, char* argv[]) {
    my_printf("In worker A\n");

    /* validate input */
    validate_input(argc, argv);

    my_printf("Worker A will execute %d iterations.\n", iterations);

    int i;
    for (i = 0 ; i < iterations ; i++ ) {
        /* Do some exchanges with neighbors */
        do_neighbor_exchange();

        /* "Compute" */
        compute(i);

        /* Write output */
        my_printf(".");
    }

    /* exit */
    return 0;
}
