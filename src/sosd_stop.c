/**
 * @file sosd_stop.c
 * Command-line utility for sending SOS daemons a shutdown directive.
 *
 * NOTE: This program does NOT require SOS to be running on the node
 *       where it is executing when a path to the SOS meetup directory
 *       is specified. In those cases it will open the .key files and
 *       connect remotely.  This allows it to run on head nodes, or
 *       analysis nodes, etc.
*/

/*
    TODO:

        [_] Make sure all includes exist.
        [_] Command line parameters are compatible with $sosd command.
        [_] Test both on-and-off-node
        [_] Shutdown data structure respects future protocol plans.

   */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(USE_MPI)
#include <mpi.h>
#endif

#include "sos.h"
#include "sos_buffer.h"
#include "sos_target.h"



#include "sos_debug.h"

#define USAGE       "USAGE:   $ sosd_stop\n" \
                    "           Default:   Shut down this node's daemon.\n" \
                    "           Options:\n" \
                    "                      [-m, --meetup <path to .key files>\n" \
                    "                       -r, --roles  <aggregator | listener | all>]  (default: all)\n" \
                    "\n" \
                    "                      [-f, --forward <bool: listeners relay shutdown to aggregator>\n" \
                    ""


typedef enum SOSD_STOP_ROLES {
    SOSD_ALL            = 0,
    SOSD_LISTENER            = 1,
    SOSD_AGGREGATOR          = 2
} SOSD_STOP_ROLES_t;

void SOSD_STOP_local_daemon(void);
void SOSD_STOP_remote_daemons(int argc, char **argv);


// Global variabls:
int rank; 
int size;
int override_fwd_shutdown_to_agg;
int senders_fwd_shutdown_setting;
char *meetup_dir;
SOSD_STOP_ROLES_t send_to_roles;


int main(int argc, char *argv[]) {
#if defined(USE_MPI)
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif

    // Set default values for global vars:
    override_fwd_shutdown_to_agg = 0;
    senders_fwd_shutdown_setting = -1;
    meetup_dir = NULL;
    send_to_roles = SOSD_ALL;

    int rc = 0;

    // Process command line arguments
    int elem, next_elem;
    for (elem = 2; i < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }
        if (      (strcmp(argv[elem], "--forward"            ) == 0)
        ||        (strcmp(argv[elem], "-f"                ) == 0)) {
            //If this option is specified we're going to override the
            //default configuration in the daemon, the only thing we're
            //checking here is what we're going to override it with.
            if (SOS_str_opt_is_enabled(argv[next_elem])) {
                override_fwd_shutdown_to_agg = 1;
                senders_fwd_shutdown_setting = 1;
            } else {
                override_fwd_shutdown_to_agg = 1;
                senders_fwd_shutdown_setting = 0;
            }
        }
        else if ( (strcmp(argv[elem], "--meetup"            ) == 0)
        ||        (strcmp(argv[elem], "-m"                  ) == 0)) {
            meetup_dir = argv[next_elem];
            if (false == SOS_dir_exists(meetup_dir)) {
                fprintf(stderr, "[sosd_stop -> ERROR] Cannot access the meetup directory specified:\n"
                                "    %s\n\n", meetup_dir);
                fflush(stderr);
                meetup_dir = NULL;
                exit(EXIT_FAILURE);
            }
        }
        else if ( (strcmp(argv[elem], "--roles"             ) == 0)
        ||        (strcmp(argv[elem], "-r"                  ) == 0)) {
            // What roles do we want to send this to?
            if ((strcmp(argv[next_elem], "aggregator") == 0)) {
                send_to_roles = SOSD_AGGREGATOR;
            } else if ((strcmp(argv[next_elem], "listener") == 0)) {
                send_to_roles = SOSD_LISTENER;
            } else if ((strcmp(argc[next_elem], "all") == 0)) {
                send_to_roles = SOSD_ALL;
            } else {
                fprintf(stderr, "[sosd_stop -> ERROR] Invalid SOS daemon role specified: %s\n\n", argv[next_elem]);
                fprintf(stderr, "%s\n", USAGE);
                fflush(stderr);
                exit(EXIT_FAILURE);
            }
           
        }
        else {
            fprintf(stderr, "[sosd_stop -> ERROR] Unknown flag: %s %s\n\n", argv[i], argv[j]);
            fprintf(stderr, "%s\n", USAGE);
            fflush(stderr);
            exit(EXIT_FAILURE);
        }
        elem = next_elem + 1;
    }

    if (meetup_dir == NULL) {
        SOSD_STOP_local_daemon();
    } else {
        // We're going to hand-roll our own SOS runtime because this may be running
        // on a node with no SOS daemon or configuration information in the environment.
        SOSD_STOP_remote_daemons(argc, argv);
    }

#if defined(USE_MPI)
    MPI_Finalize();
#endif
    return (EXIT_SUCCESS);
}



void SOSD_STOP_remote_daemons(int argc, char **argv) {
    return;
}



void SOSD_STOP_local_daemon(void) {
    SOS_msg_header  header;
    SOS_buffer     *buffer;
    SOS_runtime    *my_SOS;
    int             offset;

    my_SOS = NULL;
    SOS_init(&my_SOS, SOS_ROLE_RUNTIME_UTILITY, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (my_SOS == NULL) {
        fprintf(stderr, "sosd_stop: Unable to connect to an SOSflow"
                " daemon at port %s.\n", getenv("SOS_CMD_PORT"));
        exit(EXIT_FAILURE);
    }

    SOS_buffer_init(SOS, &buffer);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

    SOS_buffer_pack(buffer, &offset, "ii",
        override_fwd_shutdown_to_agg,
        senders_fwd_shutdown_setting);

    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    dlog(1, "Sending SOS_MSG_TYPE_SHUTDOWN ...\n");

    rc = SOS_target_connect(SOS->daemon);
    if (rc != 0) {
        dlog(1, "sosd_stop: Unable to connect to an SOSflow"
                " daemon at port %s. (after successful init)\n", getenv("SOS_CMD_PORT"));
        exit(EXIT_FAILURE);
    }
    
    SOS_target_send_msg(SOS->daemon, buffer);
    SOS_target_disconnect(SOS->daemon);

    SOS_buffer_destroy(buffer);
    dlog(1, "Done.\n");

    SOS_finalize(SOS);

    return;
}
