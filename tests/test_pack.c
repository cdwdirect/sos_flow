
#include <stdio.h>

#include "test.h"
#include "pack.h"
#include "sos.h"

int SOS_test_pack() {
    int result = 0;

    result += SOS_test_pack_int();
    result += SOS_test_pack_long();
    result += SOS_test_pack_double();
    result += SOS_test_pack_string();

    if (result == 0) {
        printf("

    return result;
}

