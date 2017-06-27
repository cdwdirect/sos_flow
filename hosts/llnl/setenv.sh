#!/bin/bash

# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
BASEDIR="$(cd "$SCRIPTPATH/../.."; pwd)"
echo $BASEDIR

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/g/g17/wood67/local/lib

export SOS_HOST_KNOWN_AS="\"LLNL (Common)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""

export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export BUILDDIR=build-llnl
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpiCC

export SOS_ENV_SET=1

#echo "Reconfiguring the build scripts..."
#cd $SOS_ROOT
#$SOS_ROOT/scripts/configure.sh -c
#cd $SOS_BUILD_DIR
#echo "-- Compile SOSflow with the following command:"
#echo ""
#echo "        make -j install"
#echo ""

