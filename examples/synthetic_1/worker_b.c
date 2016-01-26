#include "worker.h"
#include "stdio.h"
#include "stdlib.h"

int worker(int argc, char* argv[]) {
    printf("In worker B\n");
}

int compute(int iteration) { return 0; }
