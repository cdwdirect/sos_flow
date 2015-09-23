#ifndef SOS_ERROR_H
#define SOS_ERROR_H

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <execinfo.h>

#include "sos.h"
#include "sos_debug.h"

int         SOS_register_signal_handler();
void        SOS_simple_signal_handler(int sig);   /*                 (deprecated)  */
static void SOS_custom_signal_handler(int sig);   /* <--- The one to use...        */
int         SOS_unregister_signal_handler();


#endif
