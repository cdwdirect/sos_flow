/**
 * @file sosd_stop.c
 *  Utility to send the daemon a shutdown message w/out using a kill signal.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(USE_MPI)
#include <mpi.h>
#endif

#include "sos.h"
#include "sos_buffer.h"

#ifdef SOS_DEBUG
    #undef SOS_DEBUG
    #define SOS_DEBUG 1
#endif

#include "sos_debug.h"

#define USAGE "USAGE: sosd_stop [--hard stop] [--cmd_port <custom_port>]\n"

/**
 * Command-line tool for triggering voluntary daemon shutdown.
 *
 * @param argc  Number of command line options.
 * @param argv  The command line options.
 * @return      The exit status.
 */


int main(int argc, char *argv[]) {
    SOS_msg_header  header;
    SOS_buffer     *buffer;
    SOS_runtime    *my_SOS;
    int             offset;

    int rank = 0; 
    int size = 0;
#if defined(USE_MPI)
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
#endif

    int stop_hard = 0;    
    int rc = 0;

    // Process command line arguments
    int i, j;
    for (i = 2; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }
        if (      strcmp(argv[i], "--cmd_port"        ) == 0) {
            setenv("SOS_CMD_PORT", argv[j], 1);
        }
        else    {
            fprintf(stderr, "ERROR: unknown flag: %s %s\n", argv[i], argv[j]);
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }
        i = j + 1;
    }

    my_SOS = NULL;
    SOS_init(&my_SOS, SOS_ROLE_RUNTIME_UTILITY, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (my_SOS == NULL) {
        fprintf(stderr, "sosd_stop: Unable to connect to an SOSflow"
                " daemon at port %s.\n", getenv("SOS_CMD_PORT"));
        exit(EXIT_FAILURE);
    }

    SOS_SET_CONTEXT(my_SOS, "sosd_stop:main()");

    SOS_buffer_init(SOS, &buffer);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

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
#if defined(USE_MPI)
    MPI_Finalize();
#endif
    return (EXIT_SUCCESS);
}

