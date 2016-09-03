#ifndef SOS_DEBUG_H
#define SOS_DEBUG_H

#include <stdio.h>

#include "sos.h"
#include "sosd.h"

/*
 * sos_debug.h
 *
 * NOTE: A central location to switch the debug flags on and off, as
 *       well as define/re-define what happens when various debugging
 *       functions are called.
 *
 *       Include this in the .C files of the modules, it doesn't need
 *       to be in the API.
 */


/* The debug logging sensitivity level.
 *     +3 = VERY verbose
 *      0 = essential messages only (allows daemon logging)
 *     -1 = disabled in daemon/client (for production runs)  */
#define SOS_DEBUG                 0
#define SOS_DEBUG_SHOW_LOCATION   0

/* Daemon logging sensitivity. (Requires SOS_DEBUG >= 0) */
#define SOSD_DAEMON_LOG           0
#define SOSD_ECHO_TO_STDOUT       0

int     sos_daemon_lock_fptr;
FILE   *sos_daemon_log_fptr;


/* Defined in sosd.c ... */

#if (SOS_DEBUG < 0)

    /* Nullify the variadic debugging macros wherever they are in code: */
    #define dlog(level, ...)   

#else

    /* Set the behavior of the debugging macros: */
    /* Simple debug output, no locking: */
    #define dlog(level, ...);                                           \
    {                                                                   \
        char *__header;                                                 \
        __header = (char *) malloc(1024);                               \
        snprintf((__header + strlen(__header)),                         \
                 (1024 - strlen(__header)),                             \
                 "[%s]: ", SOS_WHOAMI);                                 \
        if (SOS_DEBUG_SHOW_LOCATION > 0) {                              \
            snprintf(__header, 1024, "(%s:%d)--",                       \
                     __FILE__, __LINE__ );                              \
        } else {                                                        \
            memset((__header + strlen(__header)), ' ',                  \
                   max(25 - strlen(__header), 25) );                    \
        }                                                               \
        if (SOS->role != SOS_ROLE_CLIENT) {                             \
            if (SOSD_DAEMON_LOG >= level) {                             \
                if (sos_daemon_log_fptr != NULL) {                      \
                    fprintf(sos_daemon_log_fptr, "%s", __header);       \
                    fprintf(sos_daemon_log_fptr, __VA_ARGS__);          \
                    fflush(sos_daemon_log_fptr);                        \
                }                                                       \
            }                                                           \
            if ((SOSD_DAEMON_MODE == 0) && SOSD_ECHO_TO_STDOUT) {       \
                printf("%s", __header);                                 \
                printf(__VA_ARGS__);                                    \
                fflush(stdout);                                         \
            }                                                           \
        } else {                                                        \
            if (SOS_DEBUG >= level) {                                   \
                printf("%s", __header);                                 \
                printf(__VA_ARGS__);                                    \
                fflush(stdout);                                         \
            }                                                           \
        }                                                               \
        free(__header);                                                 \
    }                                                               

#endif //DEBUG

#endif //SOS_DEBUG_H
