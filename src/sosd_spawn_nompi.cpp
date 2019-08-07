#include "sos.h"
#include "unistd.h"
#include <string>
#include <iostream>

extern "C" void send_shutdown_message(SOS_runtime *_runtime) {
    SOS_buffer     *buffer;
    SOS_msg_header  header;
    int offset;
    setenv("SOS_SHUTDOWN", "1", 1);
    // give the asynchronous sends time to flush
    usleep(1000);

    SOS_buffer_init(_runtime, &buffer);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_SHUTDOWN;
    header.msg_from = _runtime->my_guid;
    header.ref_guid = 0;

    offset = 0;
    const char * format = "iigg";
    SOS_buffer_pack(buffer, &offset, const_cast<char*>(format),
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.ref_guid);

    header.msg_size = offset;
    offset = 0;
    const char * format2 = "i";
    SOS_buffer_pack(buffer, &offset, const_cast<char*>(format2), header.msg_size);

    std::cout << "Sending SOS_MSG_TYPE_SHUTDOWN ..." << std::endl;
    SOS_send_to_daemon(buffer, buffer);
    SOS_buffer_destroy(buffer);
}


