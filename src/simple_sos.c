
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "simple_sos.h"
#include "sos.h"
#include "sos_types.h"

int              g_sos_is_online = 0;
SOS_runtime     *g_sos = NULL;
SOS_pub         *g_pub = NULL;

#define SSOS_CONFIRM_ONLINE(__where)                        \
{                                                           \
    if (g_sos_is_online == 0) {                             \
        fprintf(stderr, "SSOS (PID:%d) -- %s called, but "  \
            "SSOS is not yet online.  Doing nothing.\n",    \
            getpid(), __where);                             \
        return;                                             \
    };                                                      \
};

// Definition for stub function, as the 'simple' interface does
// not yet support receiving feedback from the SOS daemon.
void* SSOS_feedback_handler(SOS_feedback feedback, SOS_buffer *msg);

void SSOS_init(void) {
    g_sos_is_online = 0;
    int attempt = 0;

    g_sos = NULL;
    while (g_sos == NULL) {
        SOS_init(NULL, NULL,
            &g_sos, SOS_ROLE_CLIENT,
            SOS_RECEIVES_NO_FEEDBACK, NULL);

        if (g_sos == NULL) {
            attempt += 1;
            fprintf(stderr, "SSOS (PID:%d) -- Failed to connect to daemon."
                " (#: %d)\n", getpid(), attempt);
            sleep(SSOS_ATTEMPT_DELAY);
        }

        if (attempt > SSOS_ATTEMPT_MAX) {
            fprintf(stderr, "SSOS (PID:%d) -- Maximum attempts reached."
                " Giving up.\n", getpid());
            return;
        }
    }

    g_pub = NULL;
    SOS_pub_create(g_sos, &g_pub, "ssos.source", SOS_NATURE_DEFAULT);

    if (g_pub == NULL) {
        fprintf(stderr, "SSOS (PID:%d) -- Failed to create pub handle.\n",
            getpid());
    } else {
        g_sos_is_online = 1;
    }

    return;
}

void SSOS_is_online(int32_t *addr_of_flag) {
    *addr_of_flag = g_sos_is_online;
    return;
}


void SSOS_pack(const char *name, int32_t pack_type, void *pack_val) {
    SSOS_CONFIRM_ONLINE("SSOS_pack");
    SOS_val_type is_a;
    switch(pack_type) {
    case SSOS_TYPE_INT:    is_a = SOS_VAL_TYPE_INT;    break;
    case SSOS_TYPE_LONG:   is_a = SOS_VAL_TYPE_LONG;   break;
    case SSOS_TYPE_DOUBLE: is_a = SOS_VAL_TYPE_DOUBLE; break;
    case SSOS_TYPE_STRING: is_a = SOS_VAL_TYPE_STRING; break;
    default:
        fprintf(stderr, "SSOS (PID:%d) -- Invalid pack_type for SSOS_pack:"
            " %d   (Aborting pack)\n", getpid(), pack_type);
        return;
    }
    SOS_pack(g_pub, name, is_a, pack_val);
    return;
}

void SSOS_announce(void) {
    SSOS_CONFIRM_ONLINE("SSOS_announce");
    SOS_announce(g_pub);
    return;
}

void SSOS_publish(void) {
    SSOS_CONFIRM_ONLINE("SSOS_publish");
    SOS_publish(g_pub);
    return;
}

void SSOS_finalize(void) {
    SSOS_CONFIRM_ONLINE("SSOS_finalize");
    g_sos_is_online = 0;
    SOS_finalize(g_sos);
    return;
}

void* SSOS_feedback_handler(SOS_feedback feedback, SOS_buffer *msg) {
    fprintf(stderr, "SSOS (PID:%d) -- Feedback handler called,"
        " should not be.\n", getpid());
    return NULL;
}
