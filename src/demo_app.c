
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

#define USAGE "USAGE:\n"        \
    "\t./demo_app"              \
    " -i <iteration_size>"      \
    " -m <max_send_count>"      \
    " -p <pub_elem_count>"      \
    " [-d <iter_delay_usec>]\n" \
    "\n"                        \
    "\t\t       - OR -\n"       \
    "\n"                        \
    "\t./demo_app"              \
    " --sql SOS_SQL_QUERY"      \
    "    (environment variable)"\
    "\n"


#define DEFAULT_MAX_SEND_COUNT 2400
#define DEFAULT_ITERATION_SIZE 25



#include "sos.h"
#include "sos_error.h"
#include "sosa.h"


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

    int    MAX_SEND_COUNT;
    int    ITERATION_SIZE;
    int    PUB_ELEM_COUNT;
    int    DELAY_ENABLED;
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
    WAIT_FOR_FEEDBACK = 0;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-i"  ) == 0) {
            ITERATION_SIZE  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-m"  ) == 0) {
            MAX_SEND_COUNT  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-p"  ) == 0) {
            PUB_ELEM_COUNT  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-d"  ) == 0) {
            DELAY_IN_USEC = strtod(argv[next_elem], NULL);
            DELAY_ENABLED = 1;
        } else if ( strcmp(argv[elem], "--sql"  ) == 0) {
            WAIT_FOR_FEEDBACK = 1;
            SQL_QUERY = getenv(argv[next_elem]);
            if ((SQL_QUERY == NULL) || (strlen(SQL_QUERY) < 3)) {
                printf("Please set a valid SQL query in the environment"
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

    printf("demo_app : Starting...\n");

    /* Example variables. */
    char    *str_node_id  = getenv("HOSTNAME");
    char    *str_prog_ver = "1.0";
    char     var_string[100] = {0};
    int      var_int;
    double   var_double;
    int      send_shutdown = 0;
    
    my_sos = NULL;
    SOS_init( &argc, &argv, &my_sos, SOS_ROLE_CLIENT,
                SOS_RECEIVES_DIRECT_MESSAGES, DEMO_feedback_handler);

    /*
    if(my_sos == NULL) {
        printf("Unable to connect to SOS daemon. Determining whether to spawn...\n");
#if defined(USE_MPI)
        fork_exec_sosd();
#endif //defined(USE_MPI)
        send_shutdown = 1;
    }
    int repeat = 10;
    while(my_sos == NULL) {
        sleep(2);
        my_sos = NULL;
        //printf("init() trying to connect...\n");
        SOS_init( &argc, &argv, &my_sos, SOS_ROLE_CLIENT,
                SOS_RECEIVES_DIRECT_MESSAGES, DEMO_feedback_handler);

        if (my_sos != NULL) {
            printf("Connected to SOS daemon. Continuing...\n");
            break;
        } else if (--repeat < 0) {
            printf("Unable to connect to SOS daemon. Failing...\n");
            exit(1);
        }
    }
    */

    if (my_sos == NULL) {
        fprintf(stderr, "demo_app: Shutting down.\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }

    SOS_register_signal_handler(my_sos);
    SOS_SET_CONTEXT(my_sos, "demo_app.main");

    srandom(my_sos->my_guid);

    if (WAIT_FOR_FEEDBACK) {
        printf("demo_app : Sending query.  (%s)\n", SQL_QUERY);
        const char * portStr = getenv("SOS_CMD_PORT");
        if (portStr == NULL) { portStr = SOS_DEFAULT_SERVER_PORT; }
        SOSA_exec_query(my_sos, SQL_QUERY, "localhost", atoi(portStr));
        printf("demo_app : Waiting for feedback.\n");
        while(!g_done) {
            usleep(100000);
        }
    
    } else {

        dlog(1, "Registering sensitivity...\n");
        SOS_sense_register(my_sos, "example_sense");

        if (rank == 0) dlog(1, "Creating a pub...\n");

        SOS_pub_create(my_sos, &pub, "demo", SOS_NATURE_CREATE_OUTPUT);
        
        if (rank == 0) dlog(1, "  ... pub->guid  = %" SOS_GUID_FMT "\n", pub->guid);

        if (rank == 0) dlog(1, "Manually configuring some pub metadata...\n");
        strcpy (pub->prog_ver, str_prog_ver);
        pub->meta.channel     = 1;
        pub->meta.nature      = SOS_NATURE_EXEC_WORK;
        pub->meta.layer       = SOS_LAYER_APP;
        pub->meta.pri_hint    = SOS_PRI_DEFAULT;
        pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
        pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

        var_double = 0.0;

        SOS_TIME( time_start );
        int mils = 0;
        int ones = 0;
        while ((ones * PUB_ELEM_COUNT) < MAX_SEND_COUNT) {
            ones += 1;
            if ((ones%ITERATION_SIZE) == 0) {
                SOS_TIME( time_now );
                if (rank == 0) dlog(1, "  ... [ SOS_publish(%d vals) x %d ]"
                        "[ %lf seconds @ %lf / value ][ total: %d values ]\n",
                        PUB_ELEM_COUNT,
                        ITERATION_SIZE,
                        (time_now - time_start),
                        ((time_now - time_start) / (double) (PUB_ELEM_COUNT * ITERATION_SIZE)),
                        (ones * PUB_ELEM_COUNT));

                if (DELAY_ENABLED) {
                    usleep(DELAY_IN_USEC);
                }
                SOS_TIME( time_start);
            }
            if (((ones * PUB_ELEM_COUNT)%1000000) == 0) {
                if (rank == 0) dlog(1, "     ... 1,000,000 value milestone ---------\n");
            }

            for (i = 0; i < PUB_ELEM_COUNT; i++) {
                snprintf(elem_name, SOS_DEFAULT_STRING_LEN, "example_dbl_%d", i);
                SOS_pack(pub, elem_name, SOS_VAL_TYPE_DOUBLE, &var_double);
                var_double += 0.000001;
            }

            if (ones == 1) {
                if (rank == 0) { dlog(1, "Announcing\n"); }
                SOS_announce(pub);
            }
            SOS_publish(pub);
        }

        if (send_shutdown) {
#if defined(USE_MPI)
            fork_exec_sosd_shutdown();
#else
            send_shutdown_message(my_sos);
#endif //defined(USE_MPI)
        }

        SOS_publish(pub);

    }

    SOS_finalize(my_sos);
#if defined(USE_MPI)
    MPI_Finalize(); 
#endif

    printf("demo_app : Done.\n");

    return (EXIT_SUCCESS);
}
