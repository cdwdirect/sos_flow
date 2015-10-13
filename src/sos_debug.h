#ifndef SOS_DEBUG_H
#define SOS_DEBUG_H

#include <stdio.h>

#include "sos.h"
#include "sosd.h"

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

#define SOS_DEBUG                 99
#define SOS_DEBUG_SHOW_LOCATION   0

/* Should the daemon do any logging?  (yes/no)  */

#define SOSD_DAEMON_LOG           99
#define SOSD_ECHO_TO_STDOUT       1

int     sos_daemon_lock_fptr;
FILE   *sos_daemon_log_fptr;


/* Defined in sosd.c ... */

#if (SOS_DEBUG < 1)

    /* Nullify the variadic debugging macros wherever they are in code: */
    #define dlog(level, ...)

#else
    /* Set the behavior of the debugging macros: */
    /* Simple debug output, no locking: */
    #define dlog(level, ...);                                           \
    if (SOS.role == SOS_ROLE_DAEMON) {                                  \
                                      if (SOSD_DAEMON_LOG > level) {    \
            if (SOS_DEBUG_SHOW_LOCATION > 0) {                          \
                fprintf(sos_daemon_log_fptr, "(%s:%d)",                 \
                        __FILE__, __LINE__ );                           \
            }                                                           \
            fprintf(sos_daemon_log_fptr, __VA_ARGS__);                  \
            fflush(sos_daemon_log_fptr);                                \
            if ((SOSD_DAEMON_MODE == 0) && SOSD_ECHO_TO_STDOUT) {       \
                if (SOS_DEBUG_SHOW_LOCATION > 0) {                      \
                    printf("(%s:%d)", __FILE__, __LINE__ );             \
                }                                                       \
                printf(__VA_ARGS__);                                    \
                fflush(stdout);                                         \
            }                                                           \
        }                                                               \
    } else {                                                            \
        if (SOS_DEBUG > level && SOS.role != SOS_ROLE_DAEMON) {         \
            if (SOS_DEBUG_SHOW_LOCATION > 0) {                          \
                printf("(%s:%d)", __FILE__, __LINE__ );                 \
            }                                                           \
            printf(__VA_ARGS__);                                        \
            if (stdout) fflush(stdout);                                 \
        }                                                               \
    }


#endif //DEBUG

#endif //SOS_DEBUG_H
