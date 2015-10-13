#include <stdio.h>
#include <string.h>

#include "sos.h"
#include "sos_debug.h"

/* Pick one... */
#define STK   0
#define BSS   1
#define HEP   0


int main(int argc, char **argv) {
    SOS_init(&argc, &argv, SOS_ROLE_CLIENT);
    SOS_SET_WHOAMI(whoami, "main");

#if (STK > 0)
    char buffer_stack[SOS_DEFAULT_BUFFER_LEN];
    char *ptr;
    ptr = buffer_stack;
#endif

#if (BSS > 0)
    static SOS_buffer buffer_bss;
    char *ptr;
    ptr = buffer_bss.data;
#endif

#if (HEP > 0)
    char *buffer_heap;
    buffer_heap = (char *) malloc( SOS_DEFAULT_BUFFER_LEN );
    char *ptr;
    ptr = buffer_heap;
#endif

    memset(ptr, '\0', SOS_DEFAULT_BUFFER_LEN);

    dlog(0, "[%s]: sizeof(int)    = %d\n", whoami, (int) sizeof(int));
    dlog(0, "[%s]: sizeof(long)   = %d\n", whoami, (int) sizeof(long));
    dlog(0, "[%s]: sizeof(double) = %d\n", whoami, (int) sizeof(double));



    dlog(0, "[%s]: Done.\n", whoami);



#if (HEP > 0)
    free(buffer_heap);
#endif

    SOS_finalize();
    return(0);
}
