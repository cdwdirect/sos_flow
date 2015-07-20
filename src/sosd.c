/*
 *  sosd.c (daemon)
 *
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>


#include "sos.h"



#define USAGE "./sosd [-X optionx] [-Y optiony]"
#define DAEMON_NAME    "sosd"
#define WORK_DIR       "/tmp"
#define LOCK_FILE      "sosd.lock"
#define LOG_FILE       "sosd.log"
#define DAEMON_LOG     1
#define GET_TIME(now)  { struct timeval t; gettimeofday(&t, NULL); now = t.tv_sec + t.tv_usec/1000000.0; }
#define dlog(msg)      if (DAEMON_LOG) {   \
                             GET_TIME(time_now); fprintf(log_fptr, "[%s:%s @ %f] %s", \
                             DAEMON_NAME, daemon_pid_str, time_now, msg); }


/*#define dlog(msg, ...)   if (DAEMON_LOG) {				\
                             GET_TIME(time_now); fprintf(log_fptr, "[%s:%s @ %f] %s", \
                             DAEMON_NAME, daemon_pid_str, time_now, msg, ##__VA_ARGS__); }
*/
/* TODO: Make the dlog(...) macro work with printf-like varargs.
         This is most easily achieved by adding a char[] to sprint to
	 that becomes the message inserted into the fprintf().
*/



void daemon_init();
void signal_handler(int signal);

void do_sos_test();
void do_sos_call(char *qstr);



//[daemon] globals:
int keep_alive = 0;
int lock_fptr;
FILE* log_fptr;
char daemon_pid_str[256];
double time_now = 0.0;



int main(int argc, char *argv[]) {
    int i, j;

    //[command line] process args:
    for (i = 1; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "%s usage: %s\n", DAEMON_NAME, USAGE);
            exit(1);
        }
        if (strcmp(argv[i], "-X") == 0) {
	  //[optionx]: insert here
        } else if (strcmp(argv[i], "-Y") == 0) {
	  //[optiony]: insert here
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[i], argv[j]);
	}
	i = j + 1;
    }



    memset(&daemon_pid_str, '\0', 256);
    daemon_init();

    //[system logging] initialize:
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Starting daemon: %s", DAEMON_NAME);
    if (DAEMON_LOG) { log_fptr = fopen(LOG_FILE, "w"); }
    dlog("Daemon logging is on-line.\n");

    /* ----- DAEMON CODE BEGINS HERE --------------------- */



    do_sos_test();



    while (1) {}; //infinite loop

    /* -------------------------------------------------- */

    //[cleanup] should never get here:
    dlog("Exiting main() beneath the infinite loop.\n");
    closelog();
    if (DAEMON_LOG) { fclose(log_fptr); }

    return EXIT_SUCCESS;
} //end: main()




/* ******************************************* */
/* ******************************************* */



void do_sos_test() {
    dlog("[do_sos_test]: start\n");

    do_sos_call("xxx");
    do_sos_call("yyy");
    do_sos_call("zzz");

    dlog("[do_sos_test]: complete\n");
    return;
} // --------[end]: do_sos_test




void do_sos_call(char *cmdstr) {
    dlog("[do_sos_call]: start\n");
    dlog(cmdstr);
    dlog("[do_sos_call]: complete\n");
    return;
} // ---------[end]: do_sos_call



/* ******************************************* */
/* ******************************************* */


void daemon_init() {
    pid_t pid, sid;

    pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not fork() off parent process.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent

    sprintf(daemon_pid_str, "%d", getpid());
    fprintf(stderr, "Starting daemon (%s) with PID = %s\n", DAEMON_NAME, daemon_pid_str);

    //[child session] create new:
    umask(0);
    sid = setsid();
    if (sid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not acquire a session id.\n", DAEMON_NAME); 
        exit(EXIT_FAILURE);
    }
    if ((chdir(WORK_DIR)) < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not change to working directory: %s\n", \
         DAEMON_NAME, WORK_DIR);
        exit(EXIT_FAILURE);
    }

    //[lock files] create and hold to prevent multiple daemon spawn
    lock_fptr = open(LOCK_FILE, O_RDWR | O_CREAT, 0640);
    if (lock_fptr < 0) { 
        fprintf(stderr, "\nUnable to start daemon (%s): Could not access lock file %s in directory %s\n", \
         DAEMON_NAME, LOCK_FILE, WORK_DIR);
        exit(EXIT_FAILURE);
    }
    if (lockf(lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\nUnable to start daemon (%s): An instance is already running.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    write(lock_fptr, daemon_pid_str, strlen(daemon_pid_str));


    //[file handles] close unused handles:
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    //[signals] register the signals we care to trap:
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    return;
} //end: daemon_init



void signal_handler(int signal) {
    switch (signal) {

    case SIGHUP:
        //attempt to refresh network connections
        //note: some systems use SIGHUP to reset servers...
        syslog(LOG_DEBUG, "SIGHUP signal caught.");
        break;

    case SIGTERM:
        //[shutdown] close logs to write them to disk:
        syslog(LOG_DEBUG, "SIGTERM signal caught.");
        syslog(LOG_INFO, "Shutting down.\n");
        closelog();
        dlog("Caught SIGTERM, shutting down.\n");
        if (DAEMON_LOG) { fclose(log_fptr); }
        exit(0);
        break;

    }
} //end: signal_handler






/*
 * // [MPI NOTES]
 *
 * // ----- [MPI Templates]:

   int MPI_Send(void* MSG_BUF, int MSG_SIZE, MPI_Datatype MSG_TYPE, int DEST, int TAG, MPI_Comm COMM);
   int MPI_Recv(void* MSG_BUF, int BUF_SIZE, MPI_Datatype MSG_TYPE, int SOURCE, int TAG, MPI_Comm COMM, MPI_Status* STATUS);


   // ----- [MPI Types]:
   //  MPI_CHAR   MPI_SHORT   MPI_INT   MPI_LONG   MPI_LONG_LONG MPI_UNSIGNED[_CHAR, _SHORT, _LONG]
   //  MPI_FLOAT  MPI_DOUBLE  MPI_LONG_DOUBLE MPI_BYTE   MPI_PACKED

   // ----- [Manually buffered MPI]:

   char buffer[1000];
   char *buf;
   int buf_size;
   MPI_Buffer_attach(buffer, 1000);      //Only one buffer can be attached at a time.
   // ...
   // Calls to MPI_BSend() ...
   // ...
   MPI_Buffer_detach(&buf, &buf_size);

   // ----- [Calculate buffer size]:

   int data_size;
   int message_size;
   int max_rapid_messages;
   int bcast_buffer_size;
   int generous_buf_size;

   MPI_Pack_size(1, MPI_INT, comm, &data_size);
   message_size = data_size + MPI_BSEND_OVRHEAD;
   bcast_buf_size = (comm_sz - 1) * message_size;
   generous_buf_size = bcast_buf_size * max_rapid_messages;

 *
 */
