
#include <stdio.h>

#include "sos.h"
#include "test.h"
#include "test_pack.h"



int SOS_test_pack() {
    int error_total = 0;
    int pass_fail = 0;

    SOS_test_section_start(1, "pack/unpack serialization");

    SOS_test_run(2, "pack_int", SOS_test_pack_int(), pass_fail, error_total);
    SOS_test_run(2, "pack_long", SOS_test_pack_long(), pass_fail, error_total);
    SOS_test_run(2, "pack_double", SOS_test_pack_double(), pass_fail, error_total);
    SOS_test_run(2, "pack_string", SOS_test_pack_string(), pass_fail, error_total);

    SOS_test_section_report(1, "pack/unpack serialization", error_total);

    return error_total;
}

SOS_test_pack_int() {
    unsigned char buffer[1024] = {0};
    int input = 0;
    int output = 0;

    input = random();

    SOS_buffer_pack(buffer, "i", input);
    SOS_buffer_unpack(buffer, "i", &output);

    if (input == output)
        return PASS;
    else
        return FAIL;
}

SOS_test_pack_long() {
    sleep(1);
    return NOTEST;
}

SOS_test_pack_double() {
    unsigned char buffer[1024] = {0};
    double input = 0.0;
    double output = 0.0;
    double diff = 0.0;

    input = (double)((double)random() / (double)random());

    SOS_buffer_pack(buffer, "d", input);
    SOS_buffer_unpack(buffer, "d", &output);

    diff = input - output;
    if (diff < 1) { diff *= -1; }

    if (diff < 0.000000001L) {
        return PASS;
    } else {
        return FAIL;
    }
}

SOS_test_pack_string() {
    sleep(1);
    return NOTEST;
}
