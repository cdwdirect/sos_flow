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

#define USAGE "USAGE:  sosd_trigger -h <handle> -p <SOS_PAYLOAD_STRING_EVAR>\n"

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

    char *handle = NULL;
    char *payload_data = NULL;
    int   payload_size = -1;


    /* Process command line arguments: format for options is:   --argv[i] <argv[j]>    */
    int i, j;
    for (i = 1; i < argc; ) {
        if ((j = i + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }
        if (      strcmp(argv[i], "-h"        ) == 0) {
            handle = argv[j];
        } else if (      strcmp(argv[i], "-p"        ) == 0) {
            payload_data = getenv(argv[j]);
            if ((payload_data == NULL) ||
            ((payload_size = strlen(payload_data)) < 1)) {
                fprintf(stderr, "WARNING: Will not send empty payload to clients.  Set a"
                        " value to send in the %s environment variable.\n",
                        argv[j]);
                fflush(stderr);
                payload_data = calloc(1, sizeof(char));
                payload_size = 1;
            }
        }
        else    {
            fprintf(stderr, "ERROR: unknown flag: %s %s\n", argv[i], argv[j]);
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }
        i = j + 1;
    }

    if ((handle == NULL)
     || (payload_data == NULL))
    {
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
    }

    my_SOS = NULL;
    SOS_init(&my_SOS, SOS_ROLE_RUNTIME_UTILITY, SOS_RECEIVES_NO_FEEDBACK, NULL);
    if (my_SOS == NULL) {
        fprintf(stderr, "sosd_trigger: Failed to connect to the SOS daemon.\n");
        exit(EXIT_FAILURE);
    }

    SOS_SET_CONTEXT(my_SOS, "sosd_trigger:main()");

    const char * portStr = getenv("SOS_CMD_PORT");
    if (portStr == NULL) { portStr = SOS_DEFAULT_SERVER_PORT; }
    dlog(1, "Connected to sosd (daemon) on port %s ...\n", portStr);

    SOS_buffer_init(SOS, &buffer);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_TRIGGERPULL;
    header.msg_from = SOS->my_guid;
    header.ref_guid = 0;

    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    SOS_buffer_pack(buffer, &offset, "sis",
            handle,
            payload_size,
            payload_data);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &reply, 128, false);

    SOS_send_to_daemon(buffer, reply);

    SOS_buffer_destroy(buffer);
    SOS_buffer_destroy(reply);
    dlog(1, "Done.\n");

    SOS_finalize(SOS);
    return (EXIT_SUCCESS);
}

