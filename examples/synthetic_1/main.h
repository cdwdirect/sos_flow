#pragma once

#include "sos.h"

extern SOS_pub *pub;

SOS_init_wrapper(int* argc, char*** argv) {
    SOS_init(argc, argv, SOS_ROLE_CLIENT);
    char pub_name[SOS_DEFAULT_STRING_LEN] = {0};
    char app_version[SOS_DEFAULT_STRING_LEN] = {0};
    sprintf(pub_name, "SOS_EXAMPLE_1");
    sprintf(app_version, "v0.alpha");
    pub = SOS_pub_create(pub_name);
    pub->prog_ver           = strdup(app_version);
    pub->meta.channel       = 1;
    pub->meta.nature        = SOS_NATURE_EXEC_WORK;
    pub->meta.layer         = SOS_LAYER_FLOW;
    pub->meta.pri_hint      = SOS_PRI_IMMEDIATE;
    pub->meta.scope_hint    = SOS_SCOPE_SELF;
    pub->meta.retain_hint   = SOS_RETAIN_SESSION;
}

//SOS_pack(pub, "name", SOS_VAL_TYPE_INT, value);
//SOS_announce(pub);
//SOS_publish(pub);
