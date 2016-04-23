/**
 * @file sosdstop.c
 *  Utility to send the daemon a shutdown message w/out using a kill signal.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sos.h"
#include "pack_buffer.h"

#define USAGE "USAGE: stopd [--cmd_port <port>]\n"

/**
 * Command-line tool for triggering voluntary daemon shutdown.
 *
 * @param argc  Number of command line options.
 * @param argv  The command line options.
 * @return      The exit status.
 */


int main(int argc, char *argv[]) {
    SOS_msg_header  header;
    unsigned char  *msg_out;
    unsigned char  *msg_reply;
    int             buf_len;
    SOS_runtime    *SOS;

    msg_out = (unsigned char *) malloc(SOS_DEFAULT_BUFFER_LEN);
    msg_reply = (unsigned char *) malloc(SOS_DEFAULT_REPLY_LEN);
    memset(msg_out, '\0', SOS_DEFAULT_BUFFER_LEN);
    memset(msg_reply, '\0', SOS_DEFAULT_REPLY_LEN);

    /* Process command line arguments: format for options is:   --argv[i] <argv[j]>    */
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

    fprintf(stdout, "Connecting to sosd (daemon) on port %s ...\n", getenv("SOS_CMD_PORT"));

    SOS = SOS_init(&argc, &argv, SOS_ROLE_CLIENT, SOS_LAYER_SOS_RUNTIME);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
    header.msg_from = SOS->my_guid;
    header.pub_guid = 0;

    buf_len = SOS_buffer_pack(SOS, msg_out, "iigg",
                              header.msg_size,
                              header.msg_type,
                              header.msg_from,
                              header.pub_guid);

    header.msg_size = buf_len;

    SOS_buffer_pack(SOS, msg_out, "i", header.msg_size);

    fprintf(stdout, "Sending SOS_MSG_TYPE_SHUTDOWN ...\n");

    SOS_send_to_daemon(SOS, msg_out, header.msg_size, msg_reply, SOS_DEFAULT_REPLY_LEN);

    fprintf(stdout, "Done.\n");

    SOS_finalize(SOS);
    return (EXIT_SUCCESS);
}

/**
 * @mainpage sosdstop - SOS command for stopping sosd
 * @author Chad D. Wood
 * @version 0.0
 *
 * @section copying Copying
 *
 * Copyright (C) 2015 University of Oregon
 *
 * See the COPYING file for license details.
 */
