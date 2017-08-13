#!/bin/bash

# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
BASEDIR="$(cd "$SCRIPTPATH/../.."; pwd)"

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$HOME/local/lib
export PATH=$PATH:$BASEDIR/scripts

export SOS_HOST_KNOWN_AS="\"LLNL (Common)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""

export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export BUILDDIR=build-llnl
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export SOS_WORK=$HOME
export SOS_EVPATH_MEETUP=$HOME
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpiCC

mkdir -p $SOS_WORK

echo ""
echo "SOS Environment: $SOS_HOST_KNOWN_AS"
echo ""
echo "SOS_ROOT: $SOS_ROOT"
echo "SOS_WORK: $SOS_WORK"
echo ""

export SOS_ENV_SET=1

