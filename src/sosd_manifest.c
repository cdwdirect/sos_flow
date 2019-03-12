
/*
 * sosd_manifest.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define USAGE "./sosd_manifest  [-l loop_delay_usec]\n" \
              "                 [-header on]\n"         \
              "                 [-o json]\n"

#define OUTPUT_CSV   1
#define OUTPUT_JSON  2

#include "sos.h"
#include "sosa.h"

int    GLOBAL_sleep_delay;
int    GLOBAL_output_type;
int    GLOBAL_header_on;

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

    SOS_runtime *my_sos = NULL;
    SOS_init(&my_sos, SOS_ROLE_RUNTIME_UTILITY,
            SOS_RECEIVES_NO_FEEDBACK, NULL);

    if (my_sos == NULL) {
        fprintf(stderr, "sosd_manifest: Unable to contact the SOS daemon. Terminating.\n");
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
        SOSA_request_pub_manifest(
            my_sos,
            (SOSA_results **) &manifest,
            &max_frame_overall,
            pub_title_filter,
            my_sos->daemon->remote_host,
            atoi(my_sos->daemon->remote_port));

        snprintf(header_str, 2048, "manifest (max frame: %d)",
            max_frame_overall);

        SOSA_results_output_to(stdout, manifest, header_str, output_opts);

        if (GLOBAL_sleep_delay < 1) {
            printf("\n");
            printf("Manifest generated in %1.6lf seconds.\n",
                manifest->exec_duration);
        }
        
        SOSA_results_destroy(manifest);
        manifest = NULL;
        max_frame_overall = -1;

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
