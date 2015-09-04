#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <mpi.h>

#define GET_TIME(__SOS_now)  { struct timeval t; gettimeofday(&t, NULL); __SOS_now = t.tv_sec + t.tv_usec/1000000.0; }

    int size;
    int rank;

void run_test(int x);

int main(int argc, char **argv) {

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == 0) printf("Transmission events from 1 task to 1 receiver.  2048 bytes/msg.\n");
    if (rank == 0) printf("For parity with SOS, the message should be echoed and memory\nshould be allocated dynamically to receive / hold it.\n");
    if (rank == 0) printf("(That is not presently the case!)\n");
    if (rank == 0) printf("----------\n");

    run_test( 512 );
    run_test( 1024 );
    run_test( 2048 );
    run_test( 4096 );
    run_test( 8192 );
    run_test( 528244 );


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
