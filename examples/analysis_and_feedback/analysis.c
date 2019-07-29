
// NOTE: For more detailed descriptions, see comments in analysis.h

#include <stdio.h>     // Standard C headers
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "sos.h"       // SOS Core API
#include "sosa.h"      // SOS Analysis API (Query and result utilities)

#include "protocol.h"  // This demo's definition of a simple protocol
                       // shared between the client and analysis codes,
                       // allowing them to communicate the meaning of
                       // messages sent via SOS's trigger API
                       // ALSO: includes the runtime struct and state enum

#include "analysis.h"    // Function descriptions, log() definition.

// Struct containing the analysis app's state, sync lock, SOS runtime
// pointer, etc. (see protocol.h for details)
APP_runtime      analysis;
ANALYSIS_config  config;


// =====================
int main(int argc, char **argv)
{
    log("Starting up...\n");
    analysis_startup(argc, argv);

    //TODO: Go into a stateful loop, interacting with the client...
    //
    //
    log("TODO: Interact with the client.\n");

    
    log("Shutting down...\n");
    analysis_shutdown();

    log("Done.\n");
    return (EXIT_SUCCESS);
}



// =====================
void process_payload(void *payload_data)
{
    double analysis_time;
    MSG_header header;
    char *msg = msg_unroll(&header, payload_data);

    switch (header.tag) {
        case TAG_SEND_MORE:
            log("[" SOS_BLU "TAG_SEND_MORE" SOS_WHT "]"
                    " Analysis does not send data to clients on demand!"
                    " Doing nothing.\n");
            break;

        case TAG_PRINT_STR:
            log("[" SOS_BLU "TAG_PRINT_STR" SOS_WHT "]"
                    " I was told to print the following string:\n\t\"%s\"\n",
                    msg);
            break;

        case TAG_PRINT_RTT:
            SOS_TIME(analysis_time);
            log("[" SOS_BLU "TAG_PRINT_RTT" SOS_WHT "]"
                    " It took %3.5f seconds to receive and process an"
                    " analysis trigger.\n", (analysis_time - header.time));

        case TAG_SHUT_DOWN:
            log("[" SOS_BLU "TAG_SHUT_DOWN" SOS_WHT "]"
                    " Analysis does not take shutdown orders from clients!"
                    " Doing nothing.\n");
            break;

        default:
            log("[" SOS_BOLD_RED "*UNKNOWN TAG*" SOS_CLR SOS_WHT "]"
                    " Message received with an unexpected tag: %d "
                    " Doing nothing.\n",
                    header.tag);
            break;
    }
    return;
}



// =====================
void analysis_msg_handling_callback_func(void *sos_context,
        int payload_type, int payload_size, void *payload_data)
{
    SOSA_results *results = NULL;

    switch (payload_type) { 

    case SOS_FEEDBACK_TYPE_CACHE:
    case SOS_FEEDBACK_TYPE_QUERY:
        SOSA_results_init(analysis.sos, &results);
        SOSA_results_from_buffer(results, payload_data);
        log("Query results received.  results->exec_duration == %3.5f\n",
                results->exec_duration);
        SOSA_results_output_to(stdout, results,
                "Query Results", SOSA_OUTPUT_W_HEADER);
        SOSA_results_destroy(results);
        break;

    case SOS_FEEDBACK_TYPE_PAYLOAD:
        // NOTE: The SOS feedback dispatcher that invoked this callback
        //       will free the payload_data buffer when we return below.
        // NOTE: Analysis applications do not usually register sensitivity
        //       to channels that they push on, or else this routine will
        //       get invoked whenever they are triggering a payload
        //       to go out to a client. You don't need to be subscribing
        //       to a channel you're triggering.
        process_payload(payload_data);
        break;
    }

    return;
}


// =====================
void analysis_startup(int argc, char **argv) {
    config.trigger_name       = DEFAULT_TRIGGER_NAME;
    // Process command line arguments:
    int elem, next_elem;
    for (elem = 1; elem < argc; ) {
        next_elem = elem + 1;

        if ( strcmp(argv[elem], "-t"  ) == 0) {
            config.trigger_name = argv[next_elem];
        } else if (
                ( strcmp(argv[elem], "-?"  ) == 0)
             || ( strcmp(argv[elem], "-h"  ) == 0)
             || ( strcmp(argv[elem], "--help"  ) == 0))
        {
            printf("%s\n", USAGE);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    analysis.state = STATUS_INIT;
    analysis.lock  = (pthread_mutex_t *) calloc(sizeof(pthread_mutex_t), 1);
    pthread_mutex_init(analysis.lock, NULL);
    analysis.sos   = NULL;
    analysis.pub   = NULL;
    SOS_init(&analysis.sos, SOS_ROLE_CLIENT,
            SOS_RECEIVES_DIRECT_MESSAGES,
            analysis_msg_handling_callback_func);
    if (analysis.sos == NULL) {
        log("Unable to initialize SOS.\n");
        log("Please verify the SOS daemon is online."
                "  (SOS_CMD_PORT == \"%s\")\n",
                getenv("SOS_CMD_PORT"));
        pthread_mutex_destroy(analysis.lock);
        exit(EXIT_FAILURE);
    } else {
        SOS_pub_init(analysis.sos, &analysis.pub,
                "analysis_memos", SOS_NATURE_DEFAULT);
    }
    log("Registering sensitivity to messages on channel: %s\n",
            config.trigger_name);
    SOS_sense_register(analysis.sos, config.trigger_name);
    return;
}


// =====================
void analysis_shutdown(void)
{
    SOS_pub_destroy(analysis.pub);
    SOS_finalize(analysis.sos);
    pthread_mutex_destroy(analysis.lock);
    analysis.state = STATUS_DONE;

    return;
}
