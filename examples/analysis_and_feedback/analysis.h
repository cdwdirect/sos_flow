#ifndef EXAMPLES_ANALYSIS_AND_FEEDBACK_ANALYSIS_H
#define EXAMPLES_ANALYSIS_AND_FEEDBACK_ANALYSIS_H

// ---------------   -----------------------------------------------------
//         Project : Analysis and Feedback Demonstration
//    File Summary : analysis -- Query SOS and trigger payloads to client
//          Author : Chad Wood (cdw@cs.uoregon.edu) 
// ---------------
//     Description : Demonstrate the use of the SOS API as an analysis
//                   application, connecting to the SOS runtime and
//                   submitting queries, or triggering payloads to be
//                   transmitted to clients that have registered an
//                   interested in some named channel.
// ---------------   -----------------------------------------------------

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

#define USAGE "USAGE:\n"                                            \
    "\t./analysis\n"                                                \
    "\t\t[-t <trigger_name>     ]\n"

#define VERBOSE                 1
#define DEFAULT_TRIGGER_NAME    "deliver_to_analysis_application"

#define log(...) \
      if (VERBOSE) { \
          printf(SOS_CLR SOS_WHT "== " SOS_BOLD_RED "ANALYSIS" \
                 SOS_CLR SOS_WHT ": " __VA_ARGS__); \
          fflush(stdout); \
      };

typedef struct {
   char  *trigger_name;  // Where we can be reached w/trigger messages.
} ANALYSIS_config;

/* Function:  analysis_startup
 * --------------------
 * Convenience function handling basic startup stuff, cmdline arg parsing,
 * SOS init, etc. Mostly in here to keep main function clean to help new
 * users understand how to use SOS.
 *
 * argc: Count of command line arguments
 * argv: Array of strings containing command line args
 *
 * returns: Nothing.
 */
void analysis_startup(int argc, char **argv);


/* Function:  analysis_shutdown
 * ---------------------
 * Convenience function to close things down cleanly. Exists to keep main
 * looking clean to help people learn how to use SOS.
 *
 * returns: Nothing.
 */
void analysis_shutdown(void);

/*
 * Function:  analysis_msg_handling_callback_func
 * --------------------
 * Processes messages delivered from SOS. This is where query results are
 * delivered, as well as custom messages from user-defined analysis or code-
 * steering programs, as in this example.
 *
 * This callback function has a fixed function signature defined by SOS.
 *
 * SOS will only call this function once at a time, even if multiple messages
 * have arrived. Behavior is undefined if multiple SOS runtimes are launched
 * and share the same callback handler.
 *
 * SOS defines the high-level payload_type of the message, i.e. QUERY or PAYLOAD.
 *
 * Use-defined messages sent via the SOS trigger API are PAYLOAD. It is useful
 * for users to construct a shared header file with their own protocol tags
 * that can be used to coordinate what the internal data structure of their
 * messages are. One can then embed that tag as a uint32 in the first 32-bytes
 * of the message, followed by the message content. One could also utilize a
 * JSON parsing library and require that all messages be wrapped in some
 * enclosing structure with standardized fields to explain the message.
 *
 * SOS is agnostic about internal message format, it treats the payload as a
 * sequence of unsigned chars. Note that payload_data should not be carelessly
 * used as a string! For safety, SOS will allocate one extra byte beyond the
 * length of the message and initialize it to NULL, but generally speaking
 * you should embed and extract strings in the payload as a pairing of an int
 * and a sequence of characters, with the int describing the length of the
 * string to be read out.
 *
 *  sos_context:  pointer to the SOS runtime handle that was responsible
 *                for activating this message handling callback, useful
 *                for interacting with the SOS API when processing
 *                messages.
 *  payload_type: QUERY or CACHE results, or trigger PAYLOAD
 *
 *  payload_size: total length of the data (in unsigned chars),
 *                this can be ignored for QUERY and CACHE messages
 *  payload_data: pointer to data, either SOS_results or an array
 *                of unsigned chars for triggered PAYLOAD messages
 *
 *  returns: Nothing. (may change program state, though) 
 */
void analysis_msg_handling_callback_func(void *sos_context,
        int payload_type, int payload_size, void *payload_data);



/* Function:  process_payload
 * ---------------------
 * Utility function that handles the custom triggered-message protocol we've
 * established between our analysis program and this callback within the client.
 *
 * The function signature here is our own choice, SOS doesn't know or care about it.
 * 
 * returns: Nothing (may change program state, though)
 */
void process_payload(void *payload_data);






#endif
