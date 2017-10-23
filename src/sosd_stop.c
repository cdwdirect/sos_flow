/**
 * @file sosd_stop.c
 *  Utility to send the daemon a shutdown message w/out using a kill signal.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mpi.h>

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

    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int stop_hard = 0;    

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
    SOS_init(&argc, &argv, &my_SOS, SOS_ROLE_RUNTIME_UTILITY, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (my_SOS == NULL) {
        fprintf(stderr, "ERROR: sosd_stop(%d) failed to connect to the SOS daemon.\n", rank);
        exit(EXIT_FAILURE);
    }

    SOS_SET_CONTEXT(my_SOS, "sosd_stop:main()");

    char  mpi_hostname[ MPI_MAX_PROCESSOR_NAME] = {0};
    int   mpi_hostname_len;
    MPI_Get_processor_name(mpi_hostname, &mpi_hostname_len);

    dlog(1, "Connected to sosd (daemon) on port %s ...\n", mpi_hostname);

    setenv("SOS_SHUTDOWN", "1", 1);

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

    SOS_target_connect(SOS->daemon);
    SOS_target_send_msg(SOS->daemon, buffer);
    SOS_target_disconnect(SOS->daemon);

    SOS_buffer_destroy(buffer);
    dlog(1, "Done.\n");

    SOS_finalize(SOS);
    MPI_Finalize();
    return (EXIT_SUCCESS);
}

