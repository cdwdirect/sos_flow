#ifndef SOS_DEBUG_H
#define SOS_DEBUG_H

#include <stdio.h>

/*
 * sos_debug.h
 *
 * THREAD SAFETY NOTE: The LOCKING version of this *is* thread safe.  :)
 *
 *
 * NOTE: A central location to switch the debug flags on and off, as
 *       well as define/re-define what happens when various debugging
 *       functions are called.
 *
 *       Include this in the .C files of the modules, it doesn't need
 *       to be in the API.
 */


/* The debug logging sensitivity level.  5+ is VERY verbose. */
#define SOS_DEBUG 99

#ifndef SOS_DEBUG

    /* Nullify the variadic debugging macros wherever they are in code: */
    #define dlog(level, ...)

#else
/* Set the behavior of the debugging macros: */

    /* Simple debug output, no locking: */
    #define dlog(level, ...);                                           \
    if (SOS_DEBUG >= level && SOS.role != SOS_ROLE_DAEMON) {            \
        printf(__VA_ARGS__);                                            \
    }


#endif //DEBUG

#endif //SOS_DEBUG_H
