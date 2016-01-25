#include "worker.h"
#include "stdio.h"
#include "stdlib.h"

int worker(int argc, char* argv[]) {
    printf("%d of %d In worker C\n", myrank, commsize);
}

int compute(int iteration) { return 0; }
