#!/bin/bash

# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
BASEDIR="$(cd "$SCRIPTPATH/../.."; pwd)"
echo $BASEDIR

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/g/g17/wood67/local/lib
export PATH=$PATH:$BASEDIR/scripts

export SOS_HOST_KNOWN_AS="\"LLNL (Common)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""

export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export BUILDDIR=build-llnl
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export SOS_WORK=$SOS_BUILD_DIR
export SOS_EVPATH_MEETUP=$SOS_BUILD_DIR
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpiCC

export SOS_ENV_SET=1

