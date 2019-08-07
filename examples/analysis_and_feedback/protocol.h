#ifndef EXAMPLES_ANALYSIS_AND_FEEDBACK_PROTOCOL_H
#define EXAMPLES_ANALYSIS_AND_FEEDBACK_PROTOCOL_H

// ---------------   -----------------------------------------------------
//         Project : Analysis and Feedback Demonstration
//    File Summary : protocol -- Shared enums, structs, and message utilities
//          Author : Chad Wood (cdw@cs.uoregon.edu) 
// ---------------
//     Description : Both the client and analysis programs make use of these
//                   enum values to coordinate the types of messages they're
//                   sending or receiving.
//
//                   Utility functions are provided to serialize/deserialize
//                   the messages, as well as free them after use.
// ---------------   -----------------------------------------------------

#include "sos.h"


typedef enum {
    STATUS_INIT,
    STATUS_WAITING,
    STATUS_WORKING,
    STATUS_DONE
} APP_status;

typedef struct {
    pthread_mutex_t   *lock;
    APP_status         state;
    SOS_runtime       *sos;
    SOS_pub           *pub;
} APP_runtime;

typedef enum {
    TAG_SEND_MORE,
    TAG_PRINT_STR,
    TAG_PRINT_RTT,
    TAG_SHUT_DOWN
} MSG_tag;

typedef struct {
    MSG_tag          tag;
    double           time;
    size_t           size;
} MSG_header;

/* Function:  msg_create
 * ---------------------
 * Allocate space for our message and embed the header in it.
 *
 * msg_ptr_addr: Address of a void* var to fill with address we
 *               allocate for message
 * tag: What kind of message is this
 * size: How long is the message we're embedding?
 * msg: Actual message to send.
 *
 * returns: Total length of the embedded message AND header. This is what is
 *          passed to SOS_trigger().
 */
int msg_create(void **msg_ptr_addr, MSG_tag tag, size_t size, const char *msg);

/* Function:  msg_unroll
 * ---------------------
 * Unpacks the header of a message, making it easier to process.
 *
 * header_var_addr: Address of MSG_header variable to fill with values.
 * msg: Pointer to the message to be unrolled.
 *
 * returns: Pointer to the start of the embedded message.
 */
char* msg_unroll(MSG_header *header_var_addr, void *msg);

/* Function:  msg_delete
 * ---------------------
 * Wipe and free the memory the message points to.
 *
 * msg: The message to be deleted.
 *
 * returns: Nothing.
 */
void msg_delete(void *msg);


/* Function:  random_double
 * ---------------------
 * Fill a memory location with a random double-precision floating point number.
 * 
 * returns: Pseudorandomized double precision float.
 */
double random_double(void);


/* Function:  randomize_chars
 * ---------------------
 * Fill a length of contiguous memory with random characters suitable for use
 * in contexts where a NULL-terminated text string is expected.
 *
 * dest_str: Pointer to the beginning of a character array.
 * length: Number of bytes to be filled with random characters.
 *
 * returns: Nothing.
 */
void randomize_chars(char *dest_str, size_t length);


#endif
