
#include <stdio.h>

#include "sos.h"
#include "test.h"
#include "pack.h"
#include "pack_buffer.h"

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
    unsigned char buffer[1024] = {0};
    int input = 0;
    int output = 0;

    int attempt = 0;

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        input = random();
        SOS_buffer_pack(buffer, "i", input);
        SOS_buffer_unpack(buffer, "i", &output);
        if (input != output) {
            return FAIL;
        }
    }

    return PASS;
}

int SOS_test_pack_long() {
    unsigned char buffer[1024] = {0};
    long input = 0;
    long output = 0;

    int attempt = 0;

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        input = (long)random();
        SOS_buffer_pack(buffer, "l", input);
        SOS_buffer_unpack(buffer, "l", &output);
        if (input != output) {
            return FAIL;
        }
    }

    return PASS;
}

int SOS_test_pack_double() {
    char buffer[1024] = {0};
    double input = 0.0;
    double output = 0.0;
    double diff = 0.0;

    int attempt = 0;

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        input  = 0.0;
        output = 0.0;
        diff   = 0.0;

        random_double(&input);

        SOS_buffer_pack(buffer, "d", input);
        SOS_buffer_unpack(buffer, "d", &output);

        diff = input - output;
        if (diff < 0) { diff *= -1; }
        if (diff > 0.000000000001L) {
            return FAIL;
        }
    }

    return PASS;
}

int SOS_test_pack_string() {
    unsigned char buffer[1024] = {0};
    char input[512] = {0};
    char output[512] = {0};

    int attempt = 0;

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {

        random_string(input, 512);

        SOS_buffer_pack(buffer, "s", input);
        SOS_buffer_unpack(buffer, "s", output);
        if (strncmp(input, output, 512) != 0) {
            return FAIL;
        }
    }

    return PASS;
}
