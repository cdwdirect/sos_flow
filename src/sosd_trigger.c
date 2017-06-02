/**
 * @file sosd_trigger.c
 *  Utility to send the daemon a shutdown message w/out using a kill signal.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sos.h"
#include "sos_buffer.h"

#ifdef SOS_DEBUG
    #undef SOS_DEBUG
    #define SOS_DEBUG 1
#endif

#include "sos_debug.h"

#define USAGE "USAGE:  sosd_trigger -m <message>\n"

/**
 * Command-line tool for triggering feedback to listener roles..
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

    char *message = NULL;

    /* Process command line arguments: format for options is:   --argv[i] <argv[j]>    */
    int i, j;
    for (i = 1; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }
        if (      strcmp(argv[i], "-m"        ) == 0) {
            message = argv[j];
        }
        else    {
            fprintf(stderr, "ERROR: unknown flag: %s %s\n", argv[i], argv[j]);
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }
        i = j + 1;
    }

    if (message == NULL) {
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
     }

    my_SOS = NULL;
    SOS_init(&argc, &argv, &my_SOS, SOS_ROLE_RUNTIME_UTILITY, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (my_SOS == NULL) {
        fprintf(stderr, "sosd_trigger: Failed to connect to the SOS daemon.\n");
        exit(EXIT_FAILURE);
    }

    SOS_SET_CONTEXT(my_SOS, "sosd_trigger:main()");

    dlog(0, "Connected to sosd (daemon) on port %s ...\n", getenv("SOS_CMD_PORT"));

    SOS_buffer_init(SOS, &buffer);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_TRIGGERPULL;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

    offset = 0;
    SOS_buffer_pack(buffer, &offset, "iigg",
                              header.msg_size,
                              header.msg_type,
                              header.msg_from,
                              header.ref_guid);

    SOS_buffer_pack(buffer, &offset, "s", message);

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(buffer, &offset, "i", header.msg_size);

    dlog(0, "Sending SOS_MSG_TYPE_SHUTDOWN ...\n");

    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &reply, 128, false);

    SOS_send_to_daemon(buffer, reply);

    SOS_buffer_destroy(buffer);
    SOS_buffer_destroy(reply);
    dlog(0, "Done.\n");

    SOS_finalize(SOS);
    return (EXIT_SUCCESS);
}

