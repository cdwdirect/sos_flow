#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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



pid_t fork(void){
    typedef pid_t (*t_fork)(void);

    t_fork org_fork = dlsym(((void *) -1l), "fork");
    pid_t p = org_fork();

    atexit(showstats);

    if (p) {
        printf("parent(%i): child's pid == %i\n", getpid(), p);
    } else {
        printf("child(%i): parent's pid == %i\n", getpid(), getppid());
    }

    return p;
}
