
// NOTE: For more extensive descriptions, see protocol.h

#include <string.h>

#include "sos.h"
#include "protocol.h"

int
msg_create(
        void        **msg_ptr_addr,
        MSG_tag       msg_tag,
        size_t        msg_size,
        const char   *msg_data)
{
    int final_length = sizeof(MSG_header) + msg_size;

    *msg_ptr_addr = (void *) calloc((final_length + 1), 1);
    void *msg_ptr = *msg_ptr_addr;

    MSG_header header;
    
    header.tag  = msg_tag;
    header.size = msg_size;
    SOS_TIME(header.time);
   
    int offset = 0;
    memcpy(msg_ptr + offset, &header.tag,  sizeof(header.tag));  offset += sizeof(header.tag);
    memcpy(msg_ptr + offset, &header.time, sizeof(header.time)); offset += sizeof(header.time);
    memcpy(msg_ptr + offset, &header.size, sizeof(header.size)); offset += sizeof(header.size);
    memcpy(msg_ptr + offset, msg_data, msg_size);

    return final_length;
}

char *
msg_unroll(MSG_header *header_var_addr, void *msg) {
    MSG_header header;
    int offset = 0;
    memcpy(&header.tag,  msg + offset, sizeof(header.tag));  offset += sizeof(header.tag);
    memcpy(&header.time, msg + offset, sizeof(header.time)); offset += sizeof(header.time);
    memcpy(&header.size, msg + offset, sizeof(header.size)); offset += sizeof(header.size);
    header_var_addr->tag  = header.tag;
    header_var_addr->time = header.time;
    header_var_addr->size = header.size;

    return (char *) msg + offset;
};

void
msg_delete(void *msg) {
    MSG_header header;
    
    msg_unroll(&header, msg);
    randomize_chars(msg, (sizeof(MSG_header) + header.size));
    free(msg);
    
    return;    
}

double random_double(void) {
    double a;
    double b;
    double c;

    a = (double)random();
    b = (double)random();
    c = a / b;
    a = (double)random();
    c = c * a;
    a = (double)random();
    return (c * random()) / a;
}


void randomize_chars(char *dest_str, size_t size) {
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


