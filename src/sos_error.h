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
#include "sosd.h"

struct sigaction SOS_term_act;
struct sigaction SOS_ill_act;
struct sigaction SOS_abrt_act;
struct sigaction SOS_fpe_act;
struct sigaction SOS_segv_act;
struct sigaction SOS_bus_act;
struct sigaction SOS_hup_act;

extern int daemon_running;  /* from: sosd.c */


void SOS_simple_signal_handler(int signal);


void SOS_simple_signal_handler(int signal) {
    SOS_SET_WHOAMI(whoami, "SOS_simple_signal_handler");

    switch (signal) {
    case SIGTERM:
        SOSD.daemon_running = 0;
        syslog(LOG_DEBUG, "SIGTERM signal caught.");
        syslog(LOG_INFO, "Shutting down.\n");
        closelog();
        dlog(0, "[%s]: Caught SIGTERM, shutting down.\n", whoami);
        break;
    }
}





/* TODO: { SIGNAL HANDLER} Has a bug that leads to recursive death-spiral.  : (  */
static void SOS_custom_signal_handler(int sig) {
    SOS_SET_WHOAMI(whoami, "SOS_custom_signal_handler");
    static int recursion_flag;
    int crank = 0;
    int csize = 0;


    if (SOS.role == SOS_ROLE_DAEMON) {
        if (sig == SIGHUP) {
            syslog(LOG_DEBUG, "SIGHUP signal caught, shutting down.");
            dlog(0, "[%s]: Caught SIGHUP, shutting down.\n", whoami);
            SOSD.daemon_running = 0;
            return;
        } else if (sig == SIGTERM) {
            syslog(LOG_DEBUG, "SIGTERM signal caught, shutting down.");
            dlog(0, "[%s]: Caught SIGTERM, shutting down.\n", whoami);
            SOSD.daemon_running = 0;
            return;
        }
    }
  
    void *trace[32];
    size_t size, i;
    char **strings;
    
    size    = backtrace( trace, 32 );
    strings = backtrace_symbols( trace, size );
    
    dlog(0, "\n");
    dlog(0, "BACKTRACE:\n");
    dlog(0, "\n");
    dlog(0, "\n");
    
    char exe[SOS_DEFAULT_STRING_LEN];
    int len = readlink("/proc/self/exe", exe, 256);
    if (len != -1) { exe[len] = '\0'; }

    //char crash_rpt[SOS_DEFAULT_STRING_LEN];
    //sprintf(crash_rpt, "%s.crash_backtrace", exe);
    //remove(crash_rpt);
    
    // skip the first frame, it is this handler
    for( i = 1; i < size; i++ ){
        dlog(0, "%s\n", strings[i]);

        // char syscom[1024];
        // memset(syscom, '\0', 1024);
        // sprintf(syscom, "addr2line -f -e ./.libs/%s -p >> %s", exe, crash_rpt, trace[i]);
        // system(syscom);
    }
    
    dlog(0, "\n");
    dlog(0, "***************************************");
    dlog(0, "\n");
    dlog(0, "\n");
    exit(99);
}

int SOS_register_signal_handler() {
    SOS_SET_WHOAMI(whoami, "SOS_register_signal_handler");

    dlog(0, "[%s]: Register the signal handler.\n", whoami);

    /*
     *
     *
    dlog(0, "[%s]:   ... choosing: simple handler\n", whoami);
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    act.sa_handler = SOS_simple_signal_handler;
    sigaction(SIGTERM, &act, &SOS_term_act);
    return;
     */

    /*
     *  Register the more robust back-tracing handler...
     */
    dlog(0, "[%s]:   ... choosing: custom handler w/backtrack\n", whoami);
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = SOS_custom_signal_handler;
    sigaction(SIGTERM, &act, &SOS_term_act);  
    sigaction(SIGILL,  &act, &SOS_ill_act);  
    sigaction(SIGABRT, &act, &SOS_abrt_act);  
    sigaction(SIGFPE,  &act, &SOS_fpe_act);  
    sigaction(SIGSEGV, &act, &SOS_segv_act);  
    sigaction(SIGBUS,  &act, &SOS_bus_act);
    sigaction(SIGHUP,  &act, &SOS_hup_act);
    return 0;
    
}

int SOS_unregister_signal_handler() {
    sigaction(SIGTERM, &SOS_term_act, NULL);  
    sigaction(SIGILL,  &SOS_ill_act,  NULL);  
    sigaction(SIGABRT, &SOS_abrt_act, NULL);  
    sigaction(SIGFPE,  &SOS_fpe_act,  NULL);  
    sigaction(SIGSEGV, &SOS_segv_act, NULL);  
    sigaction(SIGBUS,  &SOS_bus_act,  NULL);  
    sigaction(SIGHUP,  &SOS_hup_act,  NULL);
    return 0;
}

void SOS_test_signal_handler() {
    SOS_custom_signal_handler(1);
}





