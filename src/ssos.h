#ifndef SSOS_H
#define SSOS_H

/*
 * ssos.h               "Simple SOS" common case SOSflow API wrapper,
 *                      useful for ports to additional languages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>

// The subset of data types supported by the SSOS-to-
// SOS interface.  When you pack data, you will be
// sending the void* cast address that points to the
// following memory footprints:
//   SSOS_TYPE_INT    = signed 32-bit integer
//   SSOS_TYPE_LONG   = signed 64-bit integer
//   SSOS_TYPE_DOUBLE = signed 64-bit float (IEEE 754)
//   SSOS_TYPE_STRING = null-terminated array of 1-byte char's
//
//   NOTE: Character constants in C like 'a' are encoded as
//         int32 types (C99:6.4.4.4, 883-886), though in
//         C++ they are encoded as char types (C++14x:2.13.3).
//         In practice this likely wont come up, but...
#define SSOS_TYPE_INT       1
#define SSOS_TYPE_LONG      2
#define SSOS_TYPE_DOUBLE    3
#define SSOS_TYPE_STRING    4

// Reconnect tries during failed SOS_init() call.
#define SSOS_ATTEMPT_MAX    10

// Delay between connection attempts, useful to allow
// the SOS daemon to come online when processes have
// been launched concurrently with the daemon.
// (expressed in usec)
#define SSOS_ATTEMPT_DELAY  500000

// --------------------

#ifdef __cplusplus
extern "C" {
#endif

    void SSOS_init(void);
    void SSOS_is_online(int *addr_of_int_flag);
    void SSOS_pack(char *name, int pack_type, void *addr_of_value);
    void SSOS_announce(void);
    void SSOS_publish(void);
    void SSOS_finalize(void);

#ifdef __cplusplus
}
#endif

#endif //SSOS_H
