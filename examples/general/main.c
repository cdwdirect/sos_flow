/******************************************************************************
*     SOS_Flow example.
******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include "main.h"
#include "adios_read.h"
#include <sys/types.h>
#include <unistd.h>

/* Global variables */
int comm_size = 1;
int my_rank = 0;
int iterations = 1;
char * my_name = NULL;
int num_sources = 0;
int num_sinks = 0;
char ** sources;
char ** sinks;

int mpi_writer(MPI_Comm adios_comm, char * sink);
int mpi_reader(MPI_Comm adios_comm, char * source);
int flexpath_writer(MPI_Comm adios_comm, int sink_index, bool append, bool shutdown);
int flexpath_reader(MPI_Comm adios_comm, int source_index);
int compute(int iteration);

void validate_input(int argc, char* argv[]) {
    if (argc == 1) {
        my_printf("Usage: %s --name <name> --iterations <num iterations> --writeto <reader> --readfrom <writer>\n", argv[0]);
        my_printf("\twhere --writeto and --readfrom can be used more than once.\n");
        exit(1);
    }
    /*
     * Allocate space for arrays of readers/writers. Assume that all arguments
     * are either readers or writers, and allocate that many entries.
     * This allocates too much memory, but it is a small amount. Safety first.
     */
    sources = (char**)(calloc(argc,sizeof(char*)));
    sinks = (char**)(calloc(argc,sizeof(char*)));
    /*
     * Arguments are in pairs of strings.
     */
    int index = 1;
    while ((index+1) < argc) {
        //printf("%s %s\n", argv[index], argv[index+1]);
        if (strcmp(argv[index], "--name") == 0) {
            my_name = strdup(argv[index+1]);
        } else if (strcmp(argv[index], "--iterations") == 0) {
            iterations = atoi(argv[index+1]);
        } else if (strcmp(argv[index], "--writeto") == 0) {
            sinks[num_sinks] = strdup(argv[index+1]);
            num_sinks = num_sinks + 1;
        } else if (strcmp(argv[index], "--readfrom") == 0) {
            sources[num_sources] = strdup(argv[index+1]);
            num_sources = num_sources + 1;
        }
        index = index + 2;
    }

    /*
    printf("Node %s Doing %d iterations\n", my_name, iterations);
    for (index = 0 ; index < num_sinks ; index++) {
        printf("Node %s writing to %s\n", my_name, sinks[index]);
    }
    for (index = 0 ; index < num_sources ; index++) {
        printf("Node %s reading from %s\n", my_name, sources[index]);
    }
    */
}

int main (int argc, char *argv[]) 
{
    validate_input(argc, argv);

    /*
     * Initialize TAU and start a timer for the main function.
     */
    TAU_INIT(&argc, &argv);
    TAU_PROFILE_SET_NODE(0);
    TAU_PROFILE_TIMER(tautimer, __func__, my_name, TAU_USER);
    TAU_PROFILE_START(tautimer);

    /*
     * Initialize MPI. We don't require threaded support, but with threads
     * we can send the TAU data over SOS asynchronously.
     */
    int rc = MPI_SUCCESS;
    int provided = 0;
    rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (rc != MPI_SUCCESS) {
        char *errorstring;
        int length = 0;
        MPI_Error_string(rc, errorstring, &length);
        fprintf(stderr, "Error: MPI_Init failed, rc = %d\n%s\n", rc, errorstring);
        fflush(stderr);
        exit(99);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
    my_printf("%s %s %d Running with comm_size %d\n", argv[0], my_name, getpid(), comm_size);
    MPI_Comm adios_comm;
    MPI_Comm_dup(MPI_COMM_WORLD, &adios_comm);

    adios_init ("arrays.xml", adios_comm);

    /*
     * Loop and do the things
     */
    int iter = 0;
    char tmpstr[256] = {0};
    while (iter < iterations) {
        int index;
        /*
         * Read upstream input
         */
        for (index = 0 ; index < num_sources ; index++) {
            my_printf ("%s reading from %s.\n", my_name, sources[index]);
            sprintf(tmpstr,"%s READING FROM %s", my_name, sources[index]);
            TAU_START(tmpstr);
            //mpi_reader(adios_comm, sources[index]);
            flexpath_reader(adios_comm, index);
            TAU_STOP(tmpstr);
        }
        /*
        * "compute"
        */
        my_printf ("%s computing.\n", my_name);
        compute(iter);
        /*
        if (num_sources == 0) {
        my_printf ("%s computing.\n", my_name);
            compute(iter); // compute again, if first process in workflow
        }
        */
        /*
         * Send output downstream
         */
        for (index = 0 ; index < num_sinks ; index++) {
            my_printf ("%s writing to %s.\n", my_name, sinks[index]);
            sprintf(tmpstr,"%s WRITING TO %s", my_name, sinks[index]);
            TAU_START(tmpstr);
            //mpi_writer(adios_comm, sinks[index]);
            flexpath_writer(adios_comm, index, (iter > 0), (iter == (iterations-1)));
            TAU_STOP(tmpstr);
        }
        iter++;
    }

    /*
     * Finalize ADIOS
     */
    if (num_sources > 0) {
        adios_read_finalize_method(ADIOS_READ_METHOD_FLEXPATH);
    }
    if (num_sinks > 0) {
        adios_finalize (my_rank);
    }

    /*
     * Finalize MPI
     */
    MPI_Comm_free(&adios_comm);
    MPI_Finalize();
    my_printf ("%s Done.\n", my_name);

    TAU_PROFILE_STOP(tautimer);
    return 0;
}

