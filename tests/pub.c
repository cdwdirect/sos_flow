#include <stdio.h>
#include <time.h>

#include "sos.h"
#include "test.h"
#include "pub.h"

#define ATTEMPT_MAX   2000


int SOS_test_pub() {
    int error_total = 0;
    int pass_fail = 0;

    SOS_test_section_start(1, "SOS_pub");

    SOS_test_run(2, "pub_create", SOS_test_pub_create(), pass_fail, error_total);
    SOS_test_run(2, "pub_growth", SOS_test_pub_growth(), pass_fail, error_total);
    SOS_test_run(2, "pub_duplicates", SOS_test_pub_growth(), pass_fail, error_total);
    SOS_test_run(2, "pub_values", SOS_test_pub_values(), pass_fail, error_total);

    SOS_test_section_report(1, "SOS_pub", error_total);

    return error_total;
}


int SOS_test_pub_create() {
    SOS_pub *pub;
    char pub_title[60] = {0};

    random_string(pub_title, 60);

    pub = NULL;
    pub = SOS_pub_create(TEST_sos, pub_title, SOS_NATURE_DEFAULT);

    if (pub == NULL) {
        return FAIL;
    }

    if (pub->elem_max != SOS_DEFAULT_ELEM_MAX) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    SOS_pub_destroy(pub);
    return PASS;
}




int SOS_test_pub_growth() {
    unsigned char buffer[1024] = {0};
    char some_string[25] = {0};
    char val_name[512] = {0};
    int attempt = 0;

    SOS_pub *pub;
    pub = SOS_pub_create(TEST_sos, "test_pub_growth", SOS_NATURE_DEFAULT);

    struct timespec ts;
    ts.tv_sec  = 0;
    ts.tv_nsec = 62500000;

    for (attempt = 0; attempt < (ATTEMPT_MAX / 2); attempt++) {
        random_string(some_string, 25);
        snprintf(val_name, 512, "%d%s", attempt, some_string);
        SOS_pack(pub, val_name, SOS_VAL_TYPE_INT, &attempt);
        if (SOS_RUN_MODE == SOS_ROLE_CLIENT) {
            SOS_announce(pub);
            SOS_publish(pub);
            printf("  %10d\b\b\b\b\b\b\b\b\b\b\b\b", attempt);
            nanosleep(&ts, NULL);
        }

    }

    if (pub->elem_count != (ATTEMPT_MAX / 2)) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    if (pub->elem_max < pub->elem_count) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    SOS_pub_destroy(pub);
    return PASS;
}


int SOS_test_pub_duplicates() {
    int attempt = 0;
    SOS_pub *pub;

    int    i_val = 0;
    long   l_val = 0;
    double d_val = 0.0;
    char   c_val[60] = {0};
    

    pub = SOS_pub_create(TEST_sos, "test_pub_duplicates", SOS_NATURE_DEFAULT);

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        i_val = (int) rand();
        l_val = (long) rand();
        random_double(&d_val);
        random_string(c_val, 60);

        SOS_pack(pub, "name0", SOS_VAL_TYPE_INT, &i_val);
        SOS_pack(pub, "name1", SOS_VAL_TYPE_LONG, &l_val);
        SOS_pack(pub, "name2", SOS_VAL_TYPE_DOUBLE, &d_val);
        SOS_pack(pub, "name3", SOS_VAL_TYPE_STRING, c_val);
    }

    if (pub->elem_count != 4) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    SOS_pub_destroy(pub);
    return PASS;
}


int SOS_test_pub_values() {
    int attempt = 0;
    int index = 0;
    SOS_pub *pub;
    SOS_val val;
    char   val_handle[100];
    double diff;

    int    reference_i[ATTEMPT_MAX];
    long   reference_l[ATTEMPT_MAX];
    double reference_d[ATTEMPT_MAX];
    char   reference_c[ATTEMPT_MAX][60];

    /* Generate ATTEMPT_MAX worth of values, stored locally. */
    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        reference_i[attempt] = (int) rand();
        reference_l[attempt] = (long) rand();
        random_double(&reference_d[attempt]);
        random_string(reference_c[attempt], 60);
    }

    pub = SOS_pub_create(TEST_sos, "test_pub_values", SOS_NATURE_DEFAULT);

    /* Push the values into the pub handle. */
    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "INT(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_INT, &reference_i[attempt]);
        snprintf(val_handle, 100, "LONG(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_LONG, &reference_l[attempt]);
        snprintf(val_handle, 100, "DOUBLE(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_DOUBLE, &reference_d[attempt]);
        snprintf(val_handle, 100, "STRING(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_STRING, &reference_c[attempt]);
    }

    if (pub->elem_count != (ATTEMPT_MAX * 4)) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    /* Pull the values back out, one at a time, and verify they all exist. */
    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "INT(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;
        if (val.i_val != reference_i[attempt]) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "LONG(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;

        if (val.l_val != reference_l[attempt]) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "DOUBLE(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;

        diff = val.d_val - reference_d[attempt];
        if (diff < 0) { diff *= -1; }
        if (diff > 0.000000000001L) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "STRING(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;

        if (strncmp(val.c_val, reference_c[attempt], 100) != 0) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    /* Now we do it ALL again, to cover value-updating, not just value addition... */

    /* Generate ATTEMPT_MAX worth of values, stored locally. */
    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        reference_i[attempt] = (int) rand();
        reference_l[attempt] = (long) rand();
        random_double(&reference_d[attempt]);
        random_string(reference_c[attempt], 60);
    }

    /* Push the values into the pub handle. */
    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "INT(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_INT, &reference_i[attempt]);
        snprintf(val_handle, 100, "LONG(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_LONG, &reference_l[attempt]);
        snprintf(val_handle, 100, "DOUBLE(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_DOUBLE, &reference_d[attempt]);
        snprintf(val_handle, 100, "STRING(%d)", attempt);
        SOS_pack(pub, val_handle, SOS_VAL_TYPE_STRING, &reference_c[attempt]);
    }

    if (pub->elem_count != (ATTEMPT_MAX * 4)) {
        SOS_pub_destroy(pub);
        return FAIL;
    }

    /* Pull the values back out, one at a time, and verify they all exist. */
    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "INT(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;
        if (val.i_val != reference_i[attempt]) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "LONG(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;

        if (val.l_val != reference_l[attempt]) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "DOUBLE(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;

        diff = val.d_val - reference_d[attempt];
        if (diff < 0) { diff *= -1; }
        if (diff > 0.000000000001L) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    for (attempt = 0; attempt < ATTEMPT_MAX; attempt++) {
        snprintf(val_handle, 100, "STRING(%d)", attempt);
        index = SOS_pub_search(pub, val_handle);
        if (index < 0) { SOS_pub_destroy(pub); return FAIL; }
        if (index >= pub->elem_count) { SOS_pub_destroy(pub); return FAIL; }
        val = pub->data[index]->val;

        if (strncmp(val.c_val, reference_c[attempt], 100) != 0) {
            SOS_pub_destroy(pub);
            return FAIL;
        }
    }

    SOS_pub_destroy(pub);
    return PASS;

}
