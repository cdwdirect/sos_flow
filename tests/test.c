
#include <stdio.h>
#include <string.h>

#include "sos.h"
#include "test.h"


#include "pack.h"



int SOS_test_all();
int SOS_TEST_RUN_SILENT;


int main(int argc, char *argv[]) {
    int total_errors = 0;

    if ((argc > 1) && (strcmp(argv[1], "silent") == 0)) {
        SOS_TEST_RUN_SILENT = 1;
    } else {
        SOS_TEST_RUN_SILENT = 0;
    }

    SOS_test_section_start(0, "all unit tests");

    total_errors = SOS_test_all();

    SOS_test_report_summary(0, "all unit tests", total_errors);

    return (total_errors);
}

int SOS_test_all() {
    int total_errors = 0;

    total_errors += SOS_test_pack();
    /* ... */

    return total_errors;
}
    
