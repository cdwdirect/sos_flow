#define _GNU_SOURCE

#include <stdio.h>
#include <dlfcn.h>

FILE *fopen(const char *path, const char *mode) {
    printf("In our own fopen, opening %s\n", path);

    FILE *(*original_fopen)(const char*, const char*);
    original_fopen = dlsym(RTLD_NEXT, "fopen");
    return (*original_fopen)(path, mode);
}
