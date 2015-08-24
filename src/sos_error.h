#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <execinfo.h>

//#include "sos.h"

static void sos_custom_signal_handler(int sig) {

  int crank = 0;
  int csize = 0;


  /*
  char whoami[SOS_DEFAULT_STRING_LEN];
  crank = SOS_UNIVERSE_RANK;
  csize = SOS_UNIVERSE_SIZE;

  memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);
  switch (SOS_ROLE) {
  case SOS_APP       : sprintf(whoami, "SOS_APP(%d)", SOS_RANK );       break;
  case SOS_MONITOR   : sprintf(whoami, "SOS_MONITOR(%d)", SOS_RANK );   break;
  case SOS_DB        : sprintf(whoami, "SOS_DB(%d)", SOS_RANK );        break;
  case SOS_POWSCHED  : sprintf(whoami, "SOS_POWSCHED(%d)", SOS_RANK );  break;
  case SOS_SURPLUS   : sprintf(whoami, "SOS_SURPLUS(%d)", SOS_RANK );   break;
  case SOS_ANALYTICS : sprintf(whoami, "SOS_ANALYTICS(%d)", SOS_RANK ); break;
  default            : sprintf(whoami, "UNKNOWN(%d)", SOS_RANK );   break;
  }
 

  //if(crank > 0 || sig == SIGABRT) { return; }
  fflush(stdout);
  printf("********* [%s]: CRASH: %s *********\n", whoami, strsignal(sig));
  fflush(stdout);
  exit(99);

  */
  printf("\n");
  printf("\n");

  void *trace[32];
  size_t size, i;
  char **strings;

  size    = backtrace( trace, 32 );
  /* overwrite sigaction with caller's address */
  //trace[1] = (void *)ctx.eip;
  strings = backtrace_symbols( trace, size );

  printf("\n");
  printf("BACKTRACE:");
  printf("\n");
  printf("\n");

  char exe[256];
  int len = readlink("/proc/self/exe", exe, 256);
  if (len != -1) {
    exe[len] = '\0';
  }

  // skip the first frame, it is this handler
  for( i = 1; i < size; i++ ){
   printf("%s\n", strings[i]);
   char syscom[1024];
/*
   sprintf(syscom,"addr2line -f -e %s %p", exe, trace[i]);
   system(syscom);
*/
  }

  printf("\n");
  printf("***************************************");
  printf("\n");
  printf("\n");
  fflush(stdout);
  exit(99);
}


struct sigaction term_act;
struct sigaction ill_act;
struct sigaction abrt_act;
struct sigaction fpe_act;
struct sigaction segv_act;
struct sigaction bus_act;

int sos_register_signal_handler() {
  return 0;
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = sos_custom_signal_handler;
  sigaction( SIGTERM, &act, &term_act);  
  sigaction( SIGILL, &act, &ill_act);  
  sigaction( SIGABRT, &act, &abrt_act);  
  sigaction( SIGFPE, &act, &fpe_act);  
  sigaction( SIGSEGV, &act, &segv_act);  
  sigaction( SIGBUS, &act, &bus_act);  
  return 0;
}

int sos_unregister_signal_handler() {
  sigaction( SIGTERM, &term_act, NULL);  
  sigaction( SIGILL, &ill_act, NULL);  
  sigaction( SIGABRT, &abrt_act, NULL);  
  sigaction( SIGFPE, &fpe_act, NULL);  
  sigaction( SIGSEGV, &segv_act, NULL);  
  sigaction( SIGBUS, &bus_act, NULL);  
  return 0;
}

void sos_test_signal_handler() {
  sos_custom_signal_handler(1);
}

