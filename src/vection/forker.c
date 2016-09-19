#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv) {

    printf("START\n"); fflush(stdout);

    fork();

    printf("END\n"); fflush(stdout);

    return(EXIT_SUCCESS);
}
