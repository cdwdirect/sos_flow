
#include <stdio.h>
#include <string.h>

#include "sos.h"
#include "test.h"
#include "pack.h"
#include "buffer.h"
#include "pub.h"


int SOS_test_all();
int SOS_test_show_ok() { return PASS; }

int SOS_RUN_MODE;
SOS_runtime *TEST_sos;

int main(int argc, char *argv[]) {
    int error_total = 0;
    int error_ignore = 0;

    srandom(getpid());


    if ((argc > 1) && (strcmp(argv[1], "online") == 0)) {
        SOS_RUN_MODE = SOS_ROLE_CLIENT;
    } else {
        SOS_RUN_MODE = SOS_ROLE_OFFLINE_TEST_MODE;
    }

    SOS_test_section_start(0, "SOS");

    TEST_sos = SOS_init(&argc, &argv, SOS_RUN_MODE, SOS_LAYER_SOS_RUNTIME);
    SOS_test_run(1, "SOS_init", SOS_test_show_ok(), error_ignore, error_ignore);

    error_total = SOS_test_all();

    SOS_finalize(TEST_sos);
    SOS_test_run(1, "SOS_finalize", SOS_test_show_ok(), error_ignore, error_ignore);

    SOS_test_section_report(0, "SOS", error_total);

    return (error_total);
}

int SOS_test_all() {
    int total_errors = 0;

    total_errors += SOS_test_pack();
    total_errors += SOS_test_pub();
    total_errors += SOS_test_buffer();

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
        dest_str[size - 1] = '\0';
    }
    return;
}
