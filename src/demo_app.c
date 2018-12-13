
/*
 * demo_app.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#if defined(USE_MPI)
#include <mpi.h>
#endif

#define USAGE "USAGE:\n"                                            \
    "\t./demo_app\n"                                                \
    "\t\t -i <iteration_size>\n"                                    \
    "\t\t -m <max_send_count>\n"                                    \
    "\t\t -p <pub_elem_count>\n"                                    \
    "\t\t[-d <iter_delay_usec> ]\n"                                 \
    "\t\t[-c <cache_depth>     ]\n"                                 \
    "\n"                                                            \
    "\t\t       - OR -\n"                                           \
    "\n"                                                            \
    "\t./demo_app --sql <SOS_SQL (environment variable)>\n"         \
    "\n"                                                            \
    "\t\t       - OR -\n"                                           \
    "\n"                                                            \
    "\t./demo_app --grab <VALNM_SUBSTR (environment variable)>\n"   \
     "\n"


#define DEFAULT_MAX_SEND_COUNT 2400
#define DEFAULT_ITERATION_SIZE 25



#include "sos.h"
#include "sos_error.h"
#include "sosa.h"

void random_double(double *dest_dbl);
void random_string(char   *dest_str, size_t length);


/*
#ifdef SOS_DEBUG
#undef SOS_DEBUG
#endif
#define SOS_DEBUG 1
*/

#if defined(USE_MPI)
void fork_exec_sosd(void);
void fork_exec_sosd_shutdown(void);
#endif //defined(USE_MPI)
void send_shutdown_message(SOS_runtime *runtime);

#include "sos_debug.h"

int g_done;
SOS_runtime *my_sos;

void
DEMO_feedback_handler(
        int payload_type,
        int payload_size,
        void *payload_data)
{
    SOSA_results *results = NULL;    
    
    switch (payload_type) { 

    case SOS_FEEDBACK_TYPE_QUERY:
        SOSA_results_init(my_sos, &results);
        SOSA_results_from_buffer(results, payload_data);
        SOSA_results_output_to(stdout, results,
                "Query Results", SOSA_OUTPUT_W_HEADER);
        SOSA_results_destroy(results);
        break;

    case SOS_FEEDBACK_TYPE_PAYLOAD:
        printf("demo_app : Received %d-byte payload --> \"%s\"\n",
                payload_size,
                (unsigned char *) payload_data);
        fflush(stdout);
        break;

    case SOS_FEEDBACK_TYPE_CACHE:
        SOSA_results_init(my_sos, &results);
        SOSA_results_from_buffer(results, payload_data);
        SOSA_results_output_to(stdout, results,
                "Query Results", SOSA_OUTPUT_W_HEADER);
        SOSA_results_destroy(results);
        break; 
    }
    
    g_done = 1;

    return;
}





int main(int argc, char *argv[]) {


    g_done = 0;

    int i;
    int elem;
    int next_elem;
    char pub_title[SOS_DEFAULT_STRING_LEN] = {0};
    char elem_name[SOS_DEFAULT_STRING_LEN] = {0};
    SOS_pub *pub;
    double time_now;
    double time_start;
    int    send_shutdown = 0;

    int    MAX_SEND_COUNT;
    int    ITERATION_SIZE;
    int    PUB_ELEM_COUNT;
    int    DELAY_ENABLED;
    int    PUB_CACHE_DEPTH;
    int    WAIT_FOR_FEEDBACK;
    char  *SQL_QUERY;
    double DELAY_IN_USEC;

    int rank = 0;
#if defined(USE_MPI)
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
#endif

    /* Process command-line arguments */

    MAX_SEND_COUNT  = -1;
    ITERATION_SIZE  = -1;
    PUB_ELEM_COUNT  = -1;
    DELAY_ENABLED   = 0;
    DELAY_IN_USEC   = 0;
    PUB_CACHE_DEPTH = 0;
    WAIT_FOR_FEEDBACK = 0;

    for (elem = 1; elem < argc; ) {
    /*
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }
        */
        next_elem = elem + 1;

        if ( strcmp(argv[elem], "-i"  ) == 0) {
            ITERATION_SIZE  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-m"  ) == 0) {
            MAX_SEND_COUNT  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-p"  ) == 0) {
            PUB_ELEM_COUNT  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-c"  ) == 0) {
            PUB_CACHE_DEPTH = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-d"  ) == 0) {
            DELAY_IN_USEC = strtod(argv[next_elem], NULL);
            DELAY_ENABLED = 1;
        } else if ( strcmp(argv[elem], "-s"  ) == 0) {
            printf("Sending shutdown at exit.\n");
            next_elem = elem;
            send_shutdown = 1;
        } else if ( strcmp(argv[elem], "--sql"  ) == 0) {
            WAIT_FOR_FEEDBACK = 1;
            SQL_QUERY = getenv(argv[next_elem]);
            if ((SQL_QUERY == NULL) || (strlen(SQL_QUERY) < 3)) {
                printf("Please set a valid SQL query in the environment"
                        " variable '%s' and retry.\n", argv[next_elem]);
                exit(0);
            }
        } else if ( strcmp(argv[elem], "--grab"  ) == 0) {
            WAIT_FOR_FEEDBACK = 2;
            SQL_QUERY = getenv(argv[next_elem]);
            if ((SQL_QUERY == NULL) || (strlen(SQL_QUERY) < 3)) {
                printf("Please set a valid VALUE NAME SUBSTRING in the environment"
                        " variable '%s' and retry.\n", argv[next_elem]);
                exit(0);
            }
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if (WAIT_FOR_FEEDBACK) {
        //No worries... don't need to do injection.
    } else {
        if ( (MAX_SEND_COUNT < 1)
             || (ITERATION_SIZE < 1)
             || (PUB_ELEM_COUNT < 1)
             || (DELAY_IN_USEC  < 0) )
             { fprintf(stderr, "%s\n", USAGE); exit(1); }
    }

    
    //printf("demo_app : Starting...\n");
    //printf("demo_app : Settings:\n"
    //        "\tITERATION_SIZE   = %d\n"
    //        "\tPUB_ELEM_COUNT   = %d\n"
    //        "\tMAX_SEND_COUNT   = %d\n"
    //        "\tDELAY_IN_USEC    = %lf\n",
    //        ITERATION_SIZE, PUB_ELEM_COUNT, MAX_SEND_COUNT, DELAY_IN_USEC);
    //fflush(stdout);
    

    // Example variables.
    char    *str_node_id  = getenv("HOSTNAME");
    char    *str_prog_ver = "1.0";
    char     var_string[100] = {0};
    int      var_int;
    double   var_double;
    
    my_sos = NULL;
    
    SOS_init(&my_sos, SOS_ROLE_CLIENT,
                SOS_RECEIVES_DIRECT_MESSAGES, DEMO_feedback_handler);

    if (my_sos == NULL) {
        fprintf(stderr, "demo_app: Could not connect to an SOSflow"
                " daemon at port %s. Terminating.\n", getenv("SOS_CMD_PORT"));
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    SOS_register_signal_handler(my_sos);
    SOS_SET_CONTEXT(my_sos, "demo_app.main");

    srandom(my_sos->my_guid);

    if (WAIT_FOR_FEEDBACK == 1) {
        //--- Submit an SQL query to the local daemon: 
        printf("demo_app: Sending query.  (%s)\n", SQL_QUERY);
        const char *portStr = getenv("SOS_CMD_PORT");
        if (portStr == NULL) { portStr = SOS_DEFAULT_SERVER_PORT; }
        SOSA_exec_query(my_sos, SQL_QUERY, my_sos->config.daemon_host, atoi(portStr));
        printf("demo_app: Waiting for results.\n");
        while(!g_done) {
            usleep(100000);
        }
    } else if (WAIT_FOR_FEEDBACK == 2) {
        //--- Do a CACHE_GRAB for all values w/names matching a pattern:
        printf("demo_app: Sending cache_grab."
                "  (val_name contains \"%s\")\n", SQL_QUERY);
        SOSA_cache_grab(SOS, "", SQL_QUERY, -1, -1, my_sos->config.daemon_host, 22500);
        while (!g_done) {
            usleep(100000);
        }
    } else {
        //--- Run normally, publishing example values:
        dlog(1, "Registering sensitivity...\n");
        SOS_sense_register(my_sos, "demo");

        if (rank == 0) dlog(1, "Creating a pub...\n");

        char pub_namestr[1024] = {0};
        snprintf(pub_namestr, 1024, "demo_pid_%d", getpid());

        SOS_pub_init(my_sos, &pub, pub_namestr, SOS_NATURE_DEFAULT);
        SOS_pub_config(pub, SOS_PUB_OPTION_CACHE, PUB_CACHE_DEPTH);

        if (rank == 0) dlog(1, "  ... pub->guid  = %" SOS_GUID_FMT "\n", pub->guid);
        if (rank == 0) dlog(1, "  ... pub->cache_depth = %d\n", pub->cache_depth);

        if (rank == 0) dlog(1, "Manually configuring some pub metadata...\n");
        strcpy (pub->prog_ver, str_prog_ver);
        pub->meta.channel     = 1;
        pub->meta.nature      = SOS_NATURE_EXEC_WORK;
        pub->meta.layer       = SOS_LAYER_APP;
        pub->meta.pri_hint    = SOS_PRI_DEFAULT;
        pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
        pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

        //SOS_pack(pub, "example_string", SOS_VAL_TYPE_STRING, "Hello!");
        var_double = 0.0;


        SOS_TIME( time_start );
        int mils = 0;
        int ones = 0;
        while ((ones * PUB_ELEM_COUNT) < MAX_SEND_COUNT) {
            ones += 1;
            if (DELAY_ENABLED && (ones % ITERATION_SIZE) == 0) {
                // Iteration size '-i #' is how many publishes between delays.
                usleep(DELAY_IN_USEC);
            }
            // Pack in a bunch of doubles, up to the element count specified with '-p #'
            for (i = 0; i < PUB_ELEM_COUNT; i++) {
                snprintf(elem_name, SOS_DEFAULT_STRING_LEN, "example_dbl_%d", i);
                snprintf(var_string, 100, "%3.14lf", var_double);
                //SOS_pack(pub, elem_name, SOS_VAL_TYPE_DOUBLE, &var_double);
                SOS_pack(pub, elem_name, SOS_VAL_TYPE_STRING, var_string);
                var_double += 1.00000001;
            }
            SOS_publish(pub);
            //if (ones == 100) {
            //    printf("Reconfiguring pub to have 100-deep cache.\n");
            //    SOS_pub_config(pub, SOS_PUB_OPTION_CACHE, 100);
            //}
        }


        // One last publish, for good measure.
        SOS_publish(pub);

    }

    if (send_shutdown == 1) {
        send_shutdown_message(my_sos);
    }

    SOS_finalize(my_sos);
#if defined(USE_MPI)
    MPI_Finalize(); 
#endif

    //printf("demo_app : Done.\n");

    return (EXIT_SUCCESS);
}


void random_double(double *dest_dbl) {
    double a;
    double b;
    double c;

    /* Make a random floating point value for 'input' */
    a = (double)random();
    b = (double)random();
    c = a / b;
    a = (double)random();
    c = c * a;
    a = (double)random();
    *dest_dbl = (c * random()) / a;

    return;
}


void random_string(char *dest_str, size_t size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*<>()[]{};:/,.-_=+";
    int charset_len = 0;
    int key;
    size_t n;

    charset_len = (strlen(charset) - 1);

    if (size) {
        --size;
        for (n = 0; n < size; n++) {
            key = rand() % charset_len;
            dest_str[n] = charset[key];
        }
        dest_str[size - 1] = '\0';
    }
    return;
}


