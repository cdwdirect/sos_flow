
// NOTE: For more detailed descriptions, see comments in client.h

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

#include "client.h"    // Function descriptions, log() definition.

// Struct containing the client's state, sync lock, SOS runtime
// pointer, etc. (see protocol.h for details)
APP_runtime    client;
CLIENT_config  config;


// =====================
int main(int argc, char **argv)
{
    log("Starting up.\n");
    client_startup(argc, argv);

    log("Sending initial batch of data to SOS.\n");
    send_data_to_sos();

    log("Waiting for feedback.\n");
    // Wait around doing nothing.  The client's messaging handler,
    // verbosely named client_msg_handling_callback_func(...), will
    // invoke the process_payload(...) function below whenever
    // messages arrive from the analysis application.
    while(client.state != STATUS_DONE) {
        usleep(500000);
    }

    // NOTE: The client_shutdown() function is invoked by the
    //       process_payload(...) function below, we don't need to
    //       call it explicitly here.

    log("Done.\n");
    return (EXIT_SUCCESS);
}



// =====================
void process_payload(void *payload_data)
{
    double client_time;
    MSG_header header;
    char *msg = msg_unroll(&header, payload_data);

    switch (header.tag) {
        case TAG_SEND_MORE:
            log("[" SOS_BLU "TAG_SEND_MORE" SOS_WHT "]"
                    " Sending %d more values to SOS, by request.\n",
                    config.num_values_to_send);
            send_data_to_sos();
            break;

        case TAG_PRINT_STR:
            log("[" SOS_BLU "TAG_PRINT_STR" SOS_WHT "]"
                    " I was told to print the following string:\n\t\"%s\"\n",
                    msg);
            break;

        case TAG_PRINT_RTT:
            SOS_TIME(client_time);
            log("[" SOS_BLU "TAG_PRINT_RTT" SOS_WHT "]"
                    " It took %3.5f seconds to receive and process an analysis trigger.\n",
                    (client_time - header.time));

        case TAG_SHUT_DOWN:
            log("[" SOS_BLU "TAG_SHUT_DOWN" SOS_WHT "]"
                    " Shutting down...\n");
            client_shutdown();
            break;

        default:
            log("[" SOS_BOLD_RED "*UNKNOWN TAG*" SOS_CLR SOS_WHT "]"
                    " Message received with an unexpected tag: %d"
                    " Doing nothing.\n",
                    header.tag);
            break;
    }
    return;
}


void send_data_to_sos(void)
{
    int i = 0;
    char val_name[24] = {0};

    for (i = 0; i < config.num_values_to_send; i++) {
        snprintf(val_name, 24, "%08d", i);
        SOS_pack(client.pub, val_name, SOS_VAL_TYPE_INT, &i);
    }
    SOS_publish(client.pub);
    return;
}

// =====================
void client_msg_handling_callback_func(void *sos_context,
        int payload_type, int payload_size, void *payload_data)
{
    SOSA_results *results = NULL;    

    switch (payload_type) { 

    case SOS_FEEDBACK_TYPE_CACHE:
    case SOS_FEEDBACK_TYPE_QUERY:
        SOSA_results_init(client.sos, &results);
        SOSA_results_from_buffer(results, payload_data);
        SOSA_results_output_to(stdout, results,
                "Query Results", SOSA_OUTPUT_W_HEADER);
        SOSA_results_destroy(results);
        break;

    case SOS_FEEDBACK_TYPE_PAYLOAD:
        // NOTE: The SOS feedback dispatcher that invoked this callback
        //       will free the payload_data buffer when we return below.
        process_payload(payload_data);
        break;
    }

    return;
}


// =====================
void client_startup(int argc, char **argv) {
    log("Initializing client...\n");
    config.num_values_to_send = DEFAULT_SEND_COUNT;
    config.trigger_name       = DEFAULT_TRIGGER_NAME;
    // Process command line arguments:
    int elem, next_elem;
    for (elem = 1; elem < argc; ) {
        next_elem = elem + 1;

        if ( strcmp(argv[elem], "-n"  ) == 0) {
            config.num_values_to_send = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-t"  ) == 0) {
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

    client.state = STATUS_INIT;
    client.lock  = (pthread_mutex_t *) calloc(sizeof(pthread_mutex_t), 1);
    pthread_mutex_init(client.lock, NULL);
    client.sos   = NULL;
    client.pub   = NULL;
    SOS_init(&client.sos, SOS_ROLE_CLIENT,
            SOS_RECEIVES_DIRECT_MESSAGES,
            client_msg_handling_callback_func);
    if (client.sos == NULL) {
        log("Unable to initialize SOS.\n");
        log("Please verify the SOS daemon is online."
                "  (SOS_CMD_PORT == \"%s\")\n",
                getenv("SOS_CMD_PORT"));
        pthread_mutex_destroy(client.lock);
        exit(EXIT_FAILURE);
    } else {
        SOS_pub_init(client.sos, &client.pub,
                "analysis and feedback demo", SOS_NATURE_DEFAULT);
    }
    SOS_sense_register(client.sos, config.trigger_name);
    return;
}


// =====================
void client_shutdown(void)
{
    SOS_pub_destroy(client.pub);
    SOS_finalize(client.sos);
    pthread_mutex_destroy(client.lock);
    client.state = STATUS_DONE;

    return;
}
