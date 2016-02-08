
#include <stdio.h>
#include <string.h>

#include "sos.h"
#include "test.h"
#include "pack.h"
#include "pub.h"


int SOS_test_all();
int SOS_test_show_ok() { return PASS; }
int SOS_TEST_RUN_SILENT;



int main(int argc, char *argv[]) {
    int error_total = 0;
    int error_ignore = 0;

    srandom(getpid());

    if ((argc > 1) && (strcmp(argv[1], "silent") == 0)) {
        SOS_TEST_RUN_SILENT = 1;
    } else {
        SOS_TEST_RUN_SILENT = 0;
    }

    SOS_test_section_start(0, "SOS");
    
    SOS_init(&argc, &argv, SOS_ROLE_OFFLINE_TEST_MODE);
    SOS_test_run(1, "SOS_init", SOS_test_show_ok(), error_ignore, error_ignore);

    error_total = SOS_test_all();

    SOS_finalize();
    SOS_test_run(1, "SOS_finalize", SOS_test_show_ok(), error_ignore, error_ignore);

    SOS_test_section_report(0, "SOS", error_total);

    return (error_total);
}

int SOS_test_all() {
    int total_errors = 0;

    total_errors += SOS_test_pack();
    total_errors += SOS_test_pub();

    /* ... */

    return total_errors;
}


void random_double(double *dest_dbl) {
    double a;
    double b;
    double c;

    /* Make a random floating point value for 'input' */
    a = (double)random();
    b = (double)random();
    c = a / b;
    a = (double)random();
    c = c * a;
    a = (double)random();
    *dest_dbl = (c * random()) / a;

    return;
}


void random_string(char *dest_str, size_t size) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$%^&*<>()[]{};:/,.-_=+";
    int charset_len = 0;
    int key;
    int n;

    charset_len = (strlen(charset) - 1);

    if (size) {
        --size;
        for (n = 0; n < size; n++) {
            key = rand() % charset_len;
            dest_str[n] = charset[key];
        }
        dest_str[size] = '\0';
    }
    return;
}
