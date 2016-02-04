
#include <stdio.h>
#include <string.h>

#include "sos.h"
#include "test.h"
#include "test_pack.h"



int SOS_test_all();
int SOS_TEST_RUN_SILENT;


int main(int argc, char *argv[]) {
    int error_total = 0;

    srandom(getpid());

    if ((argc > 1) && (strcmp(argv[1], "silent") == 0)) {
        SOS_TEST_RUN_SILENT = 1;
    } else {
        SOS_TEST_RUN_SILENT = 0;
    }

    SOS_test_section_start(0, "all unit tests");

    error_total = SOS_test_all();

    SOS_test_section_report(0, "all unit tests", error_total);

    return (error_total);
}

int SOS_test_all() {
    int total_errors = 0;

    total_errors += SOS_test_pack();
    /* ... */

    return total_errors;
}
    
