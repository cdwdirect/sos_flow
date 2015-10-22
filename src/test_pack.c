#include <stdio.h>
#include <string.h>

#include "sos.h"
#include "sos_debug.h"

int main(int argc, char **argv) {
    SOS_init(&argc, &argv, SOS_ROLE_CLIENT);
    SOS_SET_WHOAMI(whoami, "main");

    dlog(0, "[%s]: sizeof(int)    = %d\n", whoami, (int) sizeof(int));
    dlog(0, "[%s]: sizeof(long)   = %d\n", whoami, (int) sizeof(long));
    dlog(0, "[%s]: sizeof(double) = %d\n", whoami, (int) sizeof(double));

    dlog(0, "[%s]: Creating pubs.\n", whoami);

    SOS_pub *pub1;
    SOS_pub *pub2;

    pub1 = SOS_pub_create("ex1");
    pub2 = SOS_pub_create("ex2");

    char    *var_string   = "Hello, world!";
    int      var_int      = 10;
    double   var_double   = 88.8;

    dlog(0, "[%s]: Packing values into pub1.\n", whoami);

    SOS_pack(pub1, "example_int", SOS_VAL_TYPE_INT,    (SOS_val) var_int         );
    SOS_pack(pub1, "example_str", SOS_VAL_TYPE_STRING, (SOS_val) var_string      );
    SOS_pack(pub1, "example_dbl", SOS_VAL_TYPE_DOUBLE, (SOS_val) var_double      );

    int  buffer_len;
    char *buffer;

    buffer = (char *) malloc(SOS_DEFAULT_BUFFER_LEN);
    memset(buffer, '\0', SOS_DEFAULT_BUFFER_LEN);

    dlog(0, "[%s]: Announcing pub1 TO buffer...\n", whoami);

    SOS_announce_to_buffer(pub1, &buffer, &buffer_len);

    dlog(0, "[%s]:   ... buffer_len = %d\n", whoami, buffer_len);
    dlog(0, "[%s]: Announcing FROM buffer into pub2...\n", whoami);

    SOS_announce_from_buffer(pub2, buffer);

    dlog(0, "[%s]:   ... pub2->title = \"%s\"\n", whoami, pub2->title);
    dlog(0, "[%s]:   ... pub2->elem_count = %d\n", whoami, pub2->elem_count);

    int elem;
    for (elem = 0; elem < pub2->elem_count; elem++) {
        dlog(0, "[%s]:      ... pub2->data[%d]->name = \"%s\"\n", whoami, elem, pub2->data[elem]->name);
    }

    memset(buffer, '\0', SOS_DEFAULT_BUFFER_LEN);

    dlog(0, "[%s]: Publishing pub1 TO buffer...\n", whoami);

    SOS_publish_to_buffer(pub1, &buffer, &buffer_len);

    dlog(0, "[%s]:   ... buffer_len = %d\n", whoami, buffer_len);
    dlog(0, "[%s]: Publishing FROM buffer to pub2...\n", whoami);

    SOS_publish_from_buffer(pub2, buffer, NULL);

    for (elem = 0; elem < pub2->elem_count; elem++) {
        switch(pub2->data[elem]->type) {
        case SOS_VAL_TYPE_INT:    dlog(0, "[%s]:   ... pub2->data[%d]->val.i_val = %d   (%s)\n", whoami, elem, pub2->data[elem]->val.i_val, pub2->data[elem]->name);
        case SOS_VAL_TYPE_LONG:   dlog(0, "[%s]:   ... pub2->data[%d]->val.l_val = %ld   (%s)\n", whoami, elem, pub2->data[elem]->val.l_val, pub2->data[elem]->name);
        case SOS_VAL_TYPE_DOUBLE: dlog(0, "[%s]:   ... pub2->data[%d]->val.d_val = %lf   (%s)\n", whoami, elem, pub2->data[elem]->val.d_val, pub2->data[elem]->name);
        case SOS_VAL_TYPE_STRING: dlog(0, "[%s]:   ... pub2->data[%d]->val.s_val = %s   (%s)\n", whoami, elem, pub2->data[elem]->val.c_val, pub2->data[elem]->name);
        }
    }

    dlog(0, "[%s]: Done.\n", whoami);

    free(buffer);

    SOS_finalize();
    return(0);
}
