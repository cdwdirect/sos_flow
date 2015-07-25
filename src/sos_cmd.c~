/*
 * File.....: sos_cmd.c  (SOS command-line tool)
 *
 * Purpose..: Insert values into the SOS database from the command line,
 *            allowing the instrumentation of workflow scripts and extending
 *            access beyond C/C++/FORTRAN to Python/Perl/etc. script codes.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <mpi.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"

#define USAGE "usage: sos_cmd <host:port> <command> [<args>]                 \n"
              "                                                              \n"
	      "valid commands and arguments:                                 \n"
	      "$ sos_cmd <host:port>  ANNOUNCE    (guid --> stdout)          \n"
	      "            --val_name \"...\"                                \n"
	      "            --val_data \"...\"                                \n"
	      "            --val_type <type>                                 \n"
	      "            --val_ts_pack <time>                              \n"
	      "            --val_semantic <sem>                              \n"
	      "            --src_role <role>                                 \n"
	      "            --program_name \"...\"       - defaults -         \n"
	      "          [ --program_version \"...\" ]    (null)             \n"
	      "    opt:  [ --val_pragma \"...\"      ]    (null)             \n"
	      "          [ --val_channel \"...\"     ]    (null)             \n"
	      "          [ --pub_pragma \"...\"      ]    (null)             \n"
	      "          [ --update_priority <pri>   ]    'DEFAULT'          \n"
	      "          [ --src_layer <layer>       ]    'APP'              \n"
	      "          [ --scope_hint <layer>      ]    'GLOBAL'           \n"
	      "          [ --retention_policy <pol>  ]    'DEFAULT'          \n"
	      "          [ --job_id \"...\"          ]    (null)             \n"
	      "          [ --node_id \"...\"         ]    $HOSTNAME          \n"
	      "          [ --comm_rank <rank>        ]    '-1'               \n"
	      "          [ --process_id <id>         ]    '-1'               \n"
	      "          [ --thread_id <id>          ]    '-1'               \n"
	      "                                                              \n"
	      "$ sos_cmd <host:port> REANNOUNCE                              \n"
	      "            --val_guid <guid>                                 \n"
	      "          [ --<any_updated_args>     ]                        \n"
	      "                                                              \n"
	      "$ sos_cmd <host:port> PUBLISH                                 \n"
	      "            --val_guid <guid>                                 \n"
	      "            --val_ts_pack <timestamp>                         \n"
	      "            --val_data \"...\"                                \n"
	      "                                                              \n"
	      "$ sos_cmd <host:port> REPORT    (sos statistics and settings) \n"
	      "                                                              \n";


int main(int argc, char *argv[]) {
  int i, j;

   /* Process command line arguments. */


  /* ANNOUNCE || REANNOUNCE */
  for (i = 2; i < argc; ) {
    if ((j = i + 1) == argc) {
      fprintf(stderr, "%s\n", USAGE);
      exit(1);
    }                                                     /* someval = atoi(argv[j]); */
    if (      strcmp(argv[i], "--val_name"        ) == 0) {   }
    else if ( strcmp(argv[i], "--val_data"        ) == 0) {   }
    else if ( strcmp(argv[i], "--val_channel"     ) == 0) {   }
    else if ( strcmp(argv[i], "--val_type"        ) == 0) {   }
    else if ( strcmp(argv[i], "--val_ts_pack"     ) == 0) {   }
    else if ( strcmp(argv[i], "--val_semantic"    ) == 0) {   }
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

  /* PUBLISH */
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

  /* REPORT */



  /* .....do SOS'ey stuff... */


  return (EXIT_SUCCESS);
}

