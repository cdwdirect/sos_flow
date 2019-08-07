
#include <stdio.h>

#include "sos.h"
#include "test.h"
#include "pack.h"
#include "sos_buffer.h"

#define ATTEMPT_MAX   20000


int SOS_test_pack() {
    int error_total = 0;
    int pass_fail = 0;

    SOS_test_section_start(1, "SOS_buffer_pack");

    SOS_test_run(2, "pack_int", SOS_test_pack_int(), pass_fail, error_total);
    SOS_test_run(2, "pack_long", SOS_test_pack_long(), pass_fail, error_total);
    SOS_test_run(2, "pack_double", SOS_test_pack_double(), pass_fail, error_total);
    SOS_test_run(2, "pack_string", SOS_test_pack_string(), pass_fail, error_total);

    SOS_test_section_report(1, "SOS_buffer_pack", error_total);

    return error_total;
}


int SOS_test_pack_int() {
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

int SOS_test_pack_long() {
    SOS_buffer *buffer;
    long input = 0;
    long output = 0;
    int offset = 0;
    int attempt = 0;

    SOS_buffer_init(TEST_sos, &buffer);

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        input = (long)random();
        offset = 0; SOS_buffer_pack(buffer, &offset, "l", input);
        offset = 0; SOS_buffer_unpack(buffer, &offset, "l", &output);
        if (input != output) {
            SOS_buffer_destroy(buffer);
            return FAIL;
        }
    }

    SOS_buffer_destroy(buffer);

    return PASS;
}

int SOS_test_pack_double() {
    SOS_buffer *buffer;
    double input = 0.0;
    double output = 0.0;
    double diff = 0.0;
    int offset = 0;
    int attempt = 0;

    SOS_buffer_init(TEST_sos, &buffer);

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        input  = 0.0;
        output = 0.0;
        diff   = 0.0;

        random_double(&input);

        offset = 0; SOS_buffer_pack(buffer, &offset, "d", input);
        offset = 0; SOS_buffer_unpack(buffer, &offset, "d", &output);

        diff = input - output;
        if (diff < 0) { diff *= -1; }
        if (diff > 0.000000000001L) {
            SOS_buffer_destroy(buffer);
            return FAIL;
        }
    }

    SOS_buffer_destroy(buffer);

    return PASS;
}

int SOS_test_pack_string() {
    SOS_buffer *buffer;
    char input[512] = {0};
    char output[512] = {0};
    int offset = 0;
    int attempt = 0;

    SOS_buffer_init(TEST_sos, &buffer);

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {

        random_string(input, 512);

        offset = 0; SOS_buffer_pack(buffer, &offset, "s", input);
        offset = 0; SOS_buffer_unpack(buffer, &offset, "s", output);
        if (strncmp(input, output, 512) != 0) {
            SOS_buffer_destroy(buffer);
            return FAIL;
        }
    }

    SOS_buffer_destroy(buffer);

    return PASS;
}
