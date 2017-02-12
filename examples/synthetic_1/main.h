#pragma once

#include "sos.h"

extern SOS_pub *example_pub;
extern int myrank;
extern int commsize;
extern SOS_runtime * _runtime;

void SOS_init_wrapper(int* argc, char** argv[]) {
    _runtime = SOS_init(argc, argv, SOS_ROLE_CLIENT, SOS_LAYER_LIB);
    char pub_name[SOS_DEFAULT_STRING_LEN] = {0};
    char app_version[SOS_DEFAULT_STRING_LEN] = {0};
    sprintf(pub_name, "EXAMPLE");
    sprintf(app_version, "v0.alpha");
    // We shouldn't do this here. It should be done in SOS.
    int rank;
    int commsize;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &commsize);
    _runtime->config.comm_rank = rank;
    _runtime->config.comm_size = commsize;
    // ...but it has to be done before the pub creation.
    example_pub = SOS_pub_create(_runtime, pub_name, SOS_NATURE_DEFAULT);
    strcpy (example_pub->prog_ver, app_version);
    example_pub->meta.channel       = 1;
    example_pub->meta.nature        = SOS_NATURE_EXEC_WORK;
    example_pub->meta.layer         = SOS_LAYER_LIB;
    example_pub->meta.pri_hint      = SOS_PRI_IMMEDIATE;
    example_pub->meta.scope_hint    = SOS_SCOPE_SELF;
    example_pub->meta.retain_hint   = SOS_RETAIN_SESSION;
}

//SOS_pack(pub, "name", SOS_VAL_TYPE_INT, value);
//SOS_announce(pub);
//SOS_publish(pub);
