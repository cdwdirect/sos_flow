
#include <stdio.h>

#include "sos.h"
#include "test.h"
#include "pub.h"

#define ATTEMPT_MAX   20000


int SOS_test_pub() {
    int error_total = 0;
    int pass_fail = 0;

    SOS_test_section_start(1, "SOS_pub");

    SOS_test_run(2, "pub_growth", SOS_test_pub_growth(), pass_fail, error_total);

    SOS_test_section_report(1, "SOS_pub", error_total);

    return error_total;
}


int SOS_test_pub_growth() {
    unsigned char buffer[1024] = {0};
    int attempt = 0;

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        /* ... */
    }

    return NOTEST;
}
