#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define TREE_MAX 3
#define USAGE "./fork_tree <to_depth>"

#define SS(x) {int l;  for (l = 0; l < x; l++) { printf(" "); fflush(stdout);} };


int main(int argc, char **argv) {
    extern char **environ;
    int tree_depth = 0;
    pid_t p;



    if (argc != 2) {
        printf("USAGE: %s\n", USAGE);
        exit(EXIT_FAILURE);
    }

    tree_depth = atoi(argv[1]);

    SS(TREE_MAX - tree_depth); printf("START\n"); fflush(stdout);

    p = fork();
    if (p) {
        //parent:
    } else {
        //child:
    }
    
    //both:
    if (tree_depth > 1) {
        sprintf(argv[1], "%d", (tree_depth - 1));
        execve("./fork_tree", argv, environ);
    }
    
    SS(TREE_MAX - tree_depth); printf("END\n"); fflush(stdout);

    return(EXIT_SUCCESS);
}
