#!/bin/bash

#
#  NOTE: This has been modified from the version in the normal repo.
#         -Chad
#

# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo "This script path : $SCRIPTPATH"
BASEDIR="$(cd "$SCRIPTPATH"; pwd)"
echo "SOS base path ...: $BASEDIR"
export PROJECT_BASE="$(cd "$BASEDIR"; pwd)"
echo "Project base ....: $PROJECT_BASE"
export SOS_PYTHON="$PROJECT_BASE/python/bin/python"
echo "\$PROJECT_BASE ...: $PROJECT_BASE"
echo "\$SOS_PYTHON .....: $SOS_PYTHON"
echo "                   --> Use this to run SOS's Python modules! <--"
echo ""

export PATH=$SOSSCRIPTS:$PATH

# For tracking the environment that SOS is built in:
export SOS_HOST_KNOWN_AS="\"(default)\""
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""

export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR/sos_flow
export BUILDDIR=build-llnl
export SOS_BUILD_DIR=$SOS_ROOT/$BUILDDIR
<<<<<<< HEAD
export SOS_WORK=`pwd`
export SOS_EVPATH_MEETUP=$SOS_WORK
export SOS_DISCOVERY_DIR=$SOS_EVPATH_MEETUP
=======
export SOS_WORK=$HOME
export SOS_EVPATH_MEETUP=$SOS_WORK
>>>>>>> 70852aa8902f65689b6ff774c5e236245606b13c
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpiCC
export SOS_ENV_SET=1

export SOS_IN_MEMORY_DATABASE=TRUE
export SOS_EXPORT_DB_AT_EXIT=VERBOSE

