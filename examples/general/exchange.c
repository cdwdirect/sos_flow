#include "mpi.h"
#include "string.h"
#include "unistd.h"

void exchange(int iter) {
    int i, rank, size;
    // wait for everyone
    //MPI_Barrier(MPI_COMM_WORLD);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    // get my hostname
    const int hostlength = 1024*1024; // <-- intentionally huge, 1MB
    char hostname[hostlength];
    memset(hostname, 0, hostlength);
    gethostname(hostname, sizeof(char)*hostlength);
    // make array for all hostnames
    char * allhostnames = (char*)calloc(hostlength*size, sizeof(char));
    // get all hostnames
    MPI_Allgather(hostname, hostlength, MPI_CHAR, allhostnames,
                  hostlength*size, MPI_CHAR, MPI_COMM_WORLD);
}