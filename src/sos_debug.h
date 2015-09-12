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

#define SOS_DEBUG      99

/* Should the daemon do any logging?  (yes/no)  */

#define DAEMON_LOG     1


int     sos_daemon_lock_fptr;
FILE*   sos_daemon_log_fptr;




/* Defined in sosd.c ... */

#if (SOS_DEBUG < 1)

    /* Nullify the variadic debugging macros wherever they are in code: */
    #define dlog(level, ...)

#else
/* Set the behavior of the debugging macros: */

    /* Simple debug output, no locking: */
    #define dlog(level, ...);                                           \
    if (SOS.role == SOS_ROLE_DAEMON) {                                  \
        if ((SOS_DEBUG >= level) && DAEMON_LOG) {                       \
            fprintf(sos_daemon_log_fptr, __VA_ARGS__);                  \
            fflush(sos_daemon_log_fptr);                                \
        }                                                               \
    } else {                                                            \
        if (SOS_DEBUG >= level && SOS.role != SOS_ROLE_DAEMON) {        \
            printf(__VA_ARGS__);                                        \
            if (stdout) fflush(stdout);                                 \
        }                                                               \
    }


#endif //DEBUG

#endif //SOS_DEBUG_H
