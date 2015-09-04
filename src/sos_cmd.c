
/*
 *
 *   NOTE: At present this is a testing tool for battering the daemon with a bunch
 *         of commands.
 *
 *
 *
 *
 */








/**
 * @file sos_cmd.c
 *  Insert values into the SOS databse from the command line,
 *  allowing the instrumentation of workflow scripts and extending
 *  SOS access beyond C/C++/FORTRAN to Python/Perl/bash/etc..
 */


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"

#define USAGE "usage: sos_cmd <command> [<args>]                             \n" \
              "                                                              \n" \
	      "--- Valid Commands:  ANNOUNCE, REANNOUNCE, PUBLISH, REPORT    \n" \
	      "                                                              \n" \
	      "--- Example Commands and Arguments ---                        \n" \
	      "                                                              \n" \
	      "$ sos_cmd ANNOUNCE                  (returns guid --> stdout) \n" \
	      "            --val_name <string>                               \n" \
	      "            --val_type <SOS_type>                             \n" \
	      "            --val_data \"...\"  (quoted <string>, after type) \n" \
	      "            --val_ts_pack <double>                            \n" \
	      "            --val_semantic <int>                              \n" \
	      "            --program_name <string>                           \n" \
	      " optional:[ --program_version <string>   ] (null)             \n" \
	      "          [ --src_role <SOS_role>        ] SOS_CLIENT         \n" \
	      "          [ --val_channel <string>       ] (null)             \n" \
	      "          [ --val_pragma <string>        ] (null)             \n" \
	      "          [ --pub_pragma <string>        ] (null)             \n" \
	      "          [ --update_priority <SOS_pri>  ] SOS_PRI_DEFAULT    \n" \
	      "          [ --src_layer <SOS_layer>      ] SOS_LAYER_APP      \n" \
	      "          [ --scope_hint <SOS_scope>     ] SOS_SCOPE_DEFAULT  \n" \
	      "          [ --retain_hint <SOS_retain>   ] SOS_RRTAIN_DEFAULT \n" \
	      "          [ --job_id <string>            ] (null)             \n" \
	      "          [ --node_id <string>           ] $HOSTNAME          \n" \
	      "          [ --comm_rank <int>            ] -1                 \n" \
	      "          [ --process_id <int>           ] -1                 \n" \
	      "          [ --thread_id <int>            ] -1                 \n" \
	      "                                                              \n" \
	      "$ sos_cmd REANNOUNCE                                          \n" \
	      "            --val_guid <guid>                                 \n" \
	      "          [ --<any_updated_args>         ]                    \n" \
	      "                                                              \n" \
	      "$ sos_cmd PUBLISH                                             \n" \
	      "            --val_guid <guid>                                 \n" \
	      "            --val_ts_pack <timestamp>                         \n" \
	      "            --val_data \"...\" (<string> in quotes)           \n" \
	      "                                                              \n" \
	      "$ sos_cmd REPORT    (localhost's sos statistics and settings) \n" \
	      "                                                              \n" \
	      "                                                              \n" \
	      "                                                              \n" \
              "--- Valid SOS_* Values ---                                    \n" \
	      "                                                              \n" \
              "  <SOS_type>....: SOS_INT, SOS_LONG, SOS_DOUBLE, SOS_STRING   \n" \
	      "                                                              \n" \
	      "  <SOS_role>....: SOS_CLIENT, SOS_DAEMON, SOS_LEADER, SOS_DB  \n" \
	      "                                                              \n" \
	      "  <SOS_pri>.....: SOS_PRI_DEFAULT, SOS_PRI_LOW,               \n" \
	      "                  SOS_PRI_IMMEDIATE                           \n" \
	      "                                                              \n" \
	      "  <SOS_layer>...: SOS_LAYER_APP, SOS_LAYER_OS, SOS_LAYER_FLOW \n" \
	      "                  SOS_LAYER_LIB, SOS_LAYER_CONTROL            \n" \
	      "                                                              \n" \
	      "  <SOS_scope>...: SOS_SCOPE_DEFAULT, SOS_SCOPE_SELF,          \n" \
	      "                  SOS_SCOPE_NODE, SOS_SCOPE_ENCLAVE           \n" \
	      "                                                              \n" \
	      "  <SOS_retain>..: SOS_RETAIN_DEFAULT, SOS_RETAIN_SESSION,     \n" \
	      "                  SOS_RETAIN_IMMEDIATE                        \n" \
	      "                                                              \n" \
	      "                                                              \n"


/**
 * Command-line tool for interacting with the sos_flow system.
 *
 * @param argc  Number of command line options.
 * @param argv  The command line options.
 * @return      The exit status.
 */


int main(int argc, char *argv[]) {
    SOS_msg_header header;
    SOS_pub   *pub;
    SOS_data  *data;
    int        i, j;

    int        msg_out_len;
    char      *msg_out;
    char      *msg_reply;

    i = atoi(getenv("SOS_CMD_COUNT"));
    if (i == 0) i = 524288;

    SOS_init( &argc, &argv, SOS_ROLE_CLIENT );
    SOS_SET_WHOAMI(whoami, "sos_cmd.main");

    msg_out   = (char *) malloc( sizeof(char) * 2048 );
    msg_reply = (char *) malloc( sizeof(char) * 2048 );

    memset(msg_out,   'x',  2048);
    memset(msg_reply, '\0', 2048);
    memset(&header,   '\0', sizeof(SOS_msg_header));

    msg_out[2047] = '\0';
    msg_out_len = strlen(msg_out);

    header.msg_type = SOS_MSG_TYPE_ECHO;
    header.my_guid  = 1234;
    memcpy(msg_out, &header, sizeof(SOS_msg_header));

    for (j = 0; j < i; j++) {
        memset(msg_reply, '\0', 2048);
        SOS_send_to_daemon(msg_out, msg_out_len, msg_reply, 2048);
    }

    SOS_finalize();
    exit(0);


    /* TODO: { CMD } Don't exit immediately.  :)  At present this is to test SOS_init(...) */



    
    /* Process command line arguments... */

    /* ------------------------------------------------------------------------ */
    /* ANNOUNCE || REANNOUNCE    (NOTE: ANNOUNCE checks that ALL required values are set...) */
  
    if ( (strcmp(argv[1], "ANNOUNCE") == 0) || (strcmp(argv[1], "REANNOUNCE") == 0)) {
        for (i = 2; i < argc; ) {
            if ((j = i + 1) == argc) {
                fprintf(stderr, "%s\n", USAGE);
                exit(1);
            }                                                     /* someval = atoi(argv[j]); */
            if (      strcmp(argv[i], "--val_name"        ) == 0) { data->name = argv[j]; }
            else if ( strcmp(argv[i], "--val_type"        ) == 0) {

                if (      strcmp(argv[j], "SOS_INT") == 0 )    { data->type = SOS_VAL_TYPE_INT;    }
                else if ( strcmp(argv[j], "SOS_LONG") == 0 )   { data->type = SOS_VAL_TYPE_LONG;   }
                else if ( strcmp(argv[j], "SOS_DOUBLE") == 0 ) { data->type = SOS_VAL_TYPE_DOUBLE; }
                else if ( strcmp(argv[j], "SOS_STRING") == 0 ) { data->type = SOS_VAL_TYPE_STRING; }
                else { fprintf(stderr, "ERROR: Unknown type: %s\n", argv[j]); fprintf(stderr, "%s\n", USAGE); exit(1); }
            }

            else if ( strcmp(argv[i], "--val_data"        ) == 0) {

                switch (data->type) {
                case SOS_VAL_TYPE_INT:    sscanf(argv[j], "%d" , &(data->val.i_val)); break;
                case SOS_VAL_TYPE_LONG:   sscanf(argv[j], "%ld", &(data->val.l_val)); break;
                case SOS_VAL_TYPE_DOUBLE: sscanf(argv[j], "%lf", &(data->val.d_val)); break;
                case SOS_VAL_TYPE_STRING:
                    data->val.c_val = (char *) malloc( sizeof(char) * SOS_DEFAULT_STRING_LEN );
                    //(data[0]->val.c_val, argv[j], SOS_DEFAULT_STRING_LEN);  /* TODO */
                    break;
                default:
                    fprintf(stderr, "ERROR: --val_type MUST PRECEDE --val_data!\n");
                    fprintf(stderr, "%s\n", USAGE);
                    exit(1);
                }
            }
      
            else if ( strcmp(argv[i], "--val_channel"     ) == 0) { pub->meta.channel = argv[j]; }
            else if ( strcmp(argv[i], "--val_ts_pack"     ) == 0) { sscanf(argv[j], "%lf", &(data->time.pack)); }
            else if ( strcmp(argv[i], "--val_semantic"    ) == 0) { sscanf(argv[j], "%d", &(data->sem_hint));  }
            else if ( strcmp(argv[i], "--program_name"    ) == 0) {   }
            else if ( strcmp(argv[i], "--program_version" ) == 0) {   }
            else if ( strcmp(argv[i], "--val_pragma"      ) == 0) {   }
            else if ( strcmp(argv[i], "--pub_pragma"      ) == 0) {   }
            else if ( strcmp(argv[i], "--update_priority" ) == 0) {   }
            else if ( strcmp(argv[i], "--src_layer"       ) == 0) {   }
            else if ( strcmp(argv[i], "--src_role"        ) == 0) {   }
            else if ( strcmp(argv[i], "--scope_hint"      ) == 0) {   }
            else if ( strcmp(argv[i], "--retention_policy") == 0) {   }
            else if ( strcmp(argv[i], "--job_id"          ) == 0) {   }
            else if ( strcmp(argv[i], "--node_id"         ) == 0) {   }
            else if ( strcmp(argv[i], "--comm_rank"       ) == 0) {   }
            else if ( strcmp(argv[i], "--process_id"      ) == 0) {   }
            else if ( strcmp(argv[i], "--thread_id"       ) == 0) {   }
            /*...*/
            else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[i], argv[j]); }
            i = j + 1;
        }



    /* ------------------------------------------------------------------------ */
    /* PUBLISH */
    } else if (strcmp(argv[1], "PUBLISH") == 0) {

        for (i = 2; i < argc; ) {
            if ((j = i + 1) == argc) {
                fprintf(stderr, "%s\n", USAGE);
                exit(1);
            }                                                     /* someval = atoi(argv[j]); */
            if (      strcmp(argv[i], "--val_guid"        ) == 0) {   }
            else if ( strcmp(argv[i], "--val_data"        ) == 0) {   }
            else if ( strcmp(argv[i], "--val_ts_pack"     ) == 0) {   }
            /*...*/
            else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[i], argv[j]); }
            i = j + 1;
        }



    /* ------------------------------------------------------------------------ */
    /* REPORT */    
    } else if ( strcmp(argv[1], "REPORT") == 0 ) {

        /*
         * ...
         *
         */



    /* ------------------------------------------------------------------------ */
    /* ERROR - Invalid command supplied... */
    } else {        
        fprintf(stderr, "ERROR: Invalid COMMAND supplied.\n");
        fprintf(stderr, "%s\n", USAGE);
        exit(1);
    }


    /*
     *  TODO:{ CHAD, sos_cmd }
     *       Assemble the message, connect to the daemon, and deliver it.
     *       If it is an ANNOUNCE message type, display the GUID.
     *
     */

    
    SOS_finalize();    
    return (EXIT_SUCCESS);
}

/**
 * @mainpage sos_cmd - SOS command-line interface.
 * @author Chad D. Wood
 * @version 0.0
 *
 * @section copying Copying
 *
 * Copyright (C) 2015 University of Oregon
 *
 * See the COPYING file for license details.
 */
