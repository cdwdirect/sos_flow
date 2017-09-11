#!/bin/bash

# Absolute path this script is in, thus /home/user/bin
SOS_ENV_SCRIPT_PATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
export SOS_ROOT="$(cd "$SOS_ENV_SCRIPT_PATH/../.."; pwd)"

# For tracking the environment that SOS is built in:
export SOS_HOST_KNOWN_AS="\"LLNL (Common)\""
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""
export SOS_BUILD_NAME=build-llnl
export SOS_BUILD_DIR=$SOS_ROOT/$SOS_BUILD_NAME

export SOS_WORK=$HOME
export SOS_EVPATH_MEETUP=$SOS_WORK
export SOS_CMD_PORT=22500
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpiCC

export PATH=$SOS_ROOT/scripts:$PATH
echo ""
echo "\$SOS_HOST_KNOWN_AS ...: $SOS_HOST_KNOWN_AS"
echo "\$SOS_ROOT ............: $SOS_ROOT"
echo "\$SOS_BUILD_DIR .......: $SOS_BUILD_DIR"
echo "\$SOS_WORK ............: $SOS_WORK"
echo ""

export SOS_ENV_SET=1

