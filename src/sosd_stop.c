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

// For scanning a path's files:
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

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
                    "                      [-f, --forward <bool: listeners relay shutdown to aggregator>]\n" \
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

    //int i = 0;
    //int j = 0;
    int rc = 0;

    // Process command line arguments
    int elem = 1;
    int next_elem = 2;

    for (elem = 1; elem < (argc - 1); ) {
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
            } else if ((strcmp(argv[next_elem], "all") == 0)) {
                send_to_roles = SOSD_ALL;
            } else {
                fprintf(stderr, "[sosd_stop -> ERROR] Invalid SOS daemon role specified: %s\n\n", argv[next_elem]);
                fprintf(stderr, "%s\n", USAGE);
                fflush(stderr);
                exit(EXIT_FAILURE);
            }
           
        }
        else {
            fprintf(stderr, "[sosd_stop -> ERROR] Unknown flag: %s %s\n\n", argv[elem], argv[next_elem]);
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
    SOS_msg_header  header;
    SOS_buffer     *msg;
    SOS_runtime    *my_SOS;
    int             offset;

    SOS_socket *tgt;

    bool initialized = false;
    struct dirent *dp;
    DIR *dfd;

    if ((dfd = opendir(meetup_dir)) == NULL) {
        fprintf(stderr, "[sosd_stop -> ERROR] Cannot open meetup directory:\n\t%s\n",
                meetup_dir);
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    char filename_qfd[PATH_MAX];
    
    ssize_t length = 256;
    int bytes = 0;
    char *sosd_host;
    char *sosd_host_registered_to;
    sosd_host = calloc(length, sizeof(char));

    // For each entry in this directory
    while ((dp = readdir(dfd)) != NULL) {
        struct stat stbuf;
        sprintf(filename_qfd, "%s/%s", meetup_dir, dp->d_name);
        if (stat(filename_qfd, &stbuf) == -1) {
            fprintf(stderr, "[sosd_stop -> ERROR] Cannot stat file:\n\t%s\n",
                    filename_qfd);
            fflush(stderr);
            continue;
        }
        //Skip directories.
        if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
            continue;
        } else {
            if (    (strncmp(dp->d_name, "sosd.", 5) == 0)
                 && (strstr(dp->d_name, ".key") != NULL)) {
                // This appears to be a key file.

                FILE *keyfile;
                keyfile = fopen(filename_qfd, "r");
                // Skip cloud connection
                bytes = getline(&sosd_host, &length, keyfile);
                bytes = getline(&sosd_host, &length, keyfile);
                sosd_host[bytes - 1] = '\0';
                
                if (initialized == false) {
                    
                    my_SOS = NULL;
                    SOS_init_remote(&my_SOS, sosd_host,
                            SOS_ROLE_RUNTIME_UTILITY,
                            SOS_RECEIVES_NO_FEEDBACK,
                            NULL);

                    SOS_buffer_init(my_SOS, &msg);

                    header.msg_size = -1;
                    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
                    header.msg_from = my_SOS->my_guid;
                    header.ref_guid = 0;

                    SOS_buffer_pack(msg, &offset, "ii",
                            override_fwd_shutdown_to_agg,
                            senders_fwd_shutdown_setting);

                    offset = 0;
                    SOS_msg_zip(msg, header, 0, &offset);

                    header.msg_size = offset;
                    offset = 0;
                    SOS_msg_zip(msg, header, 0, &offset);

                    //Shut down the daemon we registered with last.
                    sosd_host_registered_to = strdup(sosd_host);

                    initialized = true;

                    continue;
                } //if: initialized == false
            
                printf("Sending shutdown message to %s...\n", sosd_host);

                SOS_target_init(my_SOS, &tgt, sosd_host, atoi(SOS_DEFAULT_SERVER_PORT));
                SOS_target_connect(tgt);
                SOS_target_send_msg(tgt, msg); 
                SOS_target_disconnect(tgt);
                SOS_target_destroy(tgt);

            } //if: this is a keyfile
        }

    } //foreach file in directory

    if (initialized) {
        printf("Sending shutdown message to %s...\n", sosd_host_registered_to);
        SOS_target_init(my_SOS, &tgt, sosd_host_registered_to, atoi(SOS_DEFAULT_SERVER_PORT));
        SOS_target_connect(tgt);
        SOS_target_send_msg(tgt, msg); 
        SOS_target_disconnect(tgt);
        SOS_target_destroy(tgt);
        
        free(sosd_host_registered_to);
    }

    free(sosd_host);

    return;
}



void SOSD_STOP_local_daemon(void) {
    SOS_msg_header  header;
    SOS_buffer     *buffer;
    SOS_runtime    *my_SOS;
    int             offset;

    int rc;
    int i;

    my_SOS = NULL;
    SOS_init(&my_SOS, SOS_ROLE_RUNTIME_UTILITY, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (my_SOS == NULL) {
        fprintf(stderr, "sosd_stop: Unable to connect to an SOSflow"
                " daemon at port %s.\n", getenv("SOS_CMD_PORT"));
        exit(EXIT_FAILURE);
    }
    SOS_buffer_init(my_SOS, &buffer);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
    header.msg_from = my_SOS->my_guid;
    header.ref_guid = 0;

    SOS_buffer_pack(buffer, &offset, "ii",
        override_fwd_shutdown_to_agg,
        senders_fwd_shutdown_setting);

    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    rc = SOS_target_connect(my_SOS->daemon);
    if (rc != 0) {
        fprintf(stderr, "sosd_stop: Unable to connect to an SOSflow"
                " daemon at port %s. (after successful init)\n", getenv("SOS_CMD_PORT"));
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    
    SOS_target_send_msg(my_SOS->daemon, buffer);
    SOS_target_disconnect(my_SOS->daemon);

    SOS_buffer_destroy(buffer);
    printf("Done.\n");

    SOS_finalize(my_SOS);

    return;
}
