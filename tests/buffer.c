
#include <stdio.h>

#include "sos.h"
#include "test.h"
#include "buffer.h"

#define ATTEMPT_MAX   20000


int SOS_test_buffer() {
    int error_total = 0;
    int pass_fail = 0;

    SOS_test_section_start(1, "SOS_buffer_grow");

    SOS_test_run(2, "buffer_grow", SOS_test_buffer_grow(), pass_fail, error_total);

    SOS_test_section_report(1, "SOS_buffer_grow", error_total);

    return error_total;
}


int SOS_test_buffer_grow() {
    SOS_buffer *buffer;
    int input = 0;
    int output = 0;
    int offset = 0;
    int attempt = 0;

    SOS_buffer_init(TEST_sos, &buffer);

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        input = random();
        offset = 0; SOS_buffer_pack(buffer, &offset, "i", input);
        offset = 0; SOS_buffer_unpack(buffer, &offset, "i", &output);
        if (input != output) {
            SOS_buffer_destroy(buffer);
            return FAIL;
        }
    }

    SOS_buffer_destroy(buffer);

    return PASS;
}

