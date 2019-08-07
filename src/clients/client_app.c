
/*
 * client_app.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define USAGE "./client_app     [-l loop_delay_usec]\n" \
              "                 [-header on]\n"         \
              "                 [-o json]\n"

#define OUTPUT_CSV   1
#define OUTPUT_JSON  2

#include "sos.h"
#include "sosa.h"

int    GLOBAL_sleep_delay;
int    GLOBAL_output_type;
int    GLOBAL_header_on;

SOS_runtime *my_sos = NULL;


// NOTE: This is the callback function that processes
//       TRIGGER/SQL/CACHE results:
void
client_feedback_handler(
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
                "client_app : (SQL QUERY)", SOSA_OUTPUT_W_HEADER);
        SOSA_results_destroy(results);
        break;

    case SOS_FEEDBACK_TYPE_PAYLOAD:
        printf("client_app : (TRIGGER) Received %d-byte payload --> \"%s\"\n",
                payload_size,
                (unsigned char *) payload_data);
        fflush(stdout);
        break;

    case SOS_FEEDBACK_TYPE_CACHE:
        SOSA_results_init(my_sos, &results);
        SOSA_results_from_buffer(results, payload_data);
        SOSA_results_output_to(stdout, results,
                "client_app : (CACHE GRAB)", SOSA_OUTPUT_W_HEADER);
        SOSA_results_destroy(results);
        break; 
    }
    
    return;
} //end: client_feedback_handler




int main(int argc, char *argv[]) {
    int   i;
    int   elem;
    int   next_elem;

    GLOBAL_header_on          = -1;
    GLOBAL_sleep_delay        = 0;
    GLOBAL_output_type        = OUTPUT_CSV;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }
        if ( strcmp(argv[elem], "-l"  ) == 0) {
            GLOBAL_sleep_delay  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-o"  ) == 0) {
            if ( strcmp(argv[next_elem], "json" ) == 0) {
                GLOBAL_output_type = OUTPUT_JSON;
            } else {
                printf("WARNING: Unknown output type specified."
                        " Defaulting to CSV.   (%s)\n", argv[next_elem]);
                GLOBAL_output_type = OUTPUT_CSV;
            }
        } else if ( strcmp(argv[elem], "-header"  ) == 0) {
            if ( strcmp(argv[next_elem], "on" ) == 0) {
                GLOBAL_header_on = 1;
            }
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n",
                    argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    SOS_init(&my_sos, SOS_ROLE_CLIENT,
            SOS_RECEIVES_DIRECT_MESSAGES,
            client_feedback_handler);

    if (my_sos == NULL) {
        fprintf(stderr, "client_app: Unable to contact the SOS daemon. Terminating.\n");
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    
    SOS_buffer *request;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(my_sos, &request,
            SOS_DEFAULT_BUFFER_MAX, false);
    SOS_buffer_init_sized_locking(my_sos, &reply,
            SOS_DEFAULT_BUFFER_MAX, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_MANIFEST;
    header.msg_from = my_sos->my_guid;
    header.ref_guid = 0;

    int offset = 0;
    SOS_msg_zip(request, header, 0, &offset);
    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(request, header, 0, &offset);

    int output_opts = 0;
    if (GLOBAL_output_type == OUTPUT_CSV) {
        output_opts = output_opts | SOSA_OUTPUT_DEFAULT;
    }
    if (GLOBAL_output_type == OUTPUT_JSON) {
        output_opts = output_opts | SOSA_OUTPUT_JSON;
    }
    if (GLOBAL_header_on > 0) {
        output_opts = output_opts | SOSA_OUTPUT_W_HEADER;
    }

    SOSA_results *manifest      = NULL;
    int max_frame_overall       = -1;
    char header_str[2048]       = {0};

    // Use this to restrict to just "TAU" stuff, for example.
    char pub_title_filter[2048] = {0};

    while (true) {

        manifest = NULL;
        SOSA_results_init(my_sos, &manifest);
        SOSA_request_pub_manifest(
            my_sos,
            (SOSA_results *) manifest,
            &max_frame_overall,
            pub_title_filter,
            my_sos->daemon->remote_host,
            atoi(my_sos->daemon->remote_port));

        snprintf(header_str, 2048, "manifest (max frame: %d)",
            max_frame_overall);

        SOSA_results_output_to(stdout, manifest, header_str, output_opts);

        if (GLOBAL_sleep_delay < 1) {
            printf("Manifest generated in %1.6lf seconds.\n",
                manifest->exec_duration);
        }
        
        SOSA_results_destroy(manifest);
        manifest = NULL;
        max_frame_overall = -1;

        // ----------------------------------------
        // NOTE: Here is where we would add query/blocking
        //       logic based on what we see in the manifest.
        //
        // EXAMPLE: Could do a g_done global boolean used
        //          as a latch w/the callback function above,
        //          the way that demo_app.c does it.
        //                  - OR -
        //          Look at ssos.c for an example (RECOMMENDED)
        //          of a "results pool" that has a counter and
        //          a function to claim an element from it in a
        //          thread-safe way. This facilitates non-
        //          blocking client_app <-> daemon interaction
        //          and multi-threaded analysis in client_app.c



        //...YOUR CODE HERE.... 



        //
        // ----------------------------------------
        //
        if (GLOBAL_sleep_delay > 0) {
            usleep(GLOBAL_sleep_delay);
        } else {
            // We're not looping, just break out of this while() loop.
            break;
        }

    }//while

    SOS_buffer_destroy(request);
    SOS_buffer_destroy(reply);
    SOS_finalize(my_sos);

    return (EXIT_SUCCESS);
}
