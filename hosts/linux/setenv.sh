# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
BASEDIR="$(cd "$SCRIPTPATH/../.."; pwd)"
echo $BASEDIR

export SOS_HOST_KNOWN_AS="\"Linux (Generic)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""
export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export PATH="${PATH}:$SOS_ROOT/scripts"
export BUILDDIR=build
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpicxx

if [ "x$PYTHONPATH" == "x" ] ; then
  PYTHONPATH=$SOS_ROOT/src/python
else
  PYTHONPATH=$SOS_ROOT/src/python:$PYTHONPATH
fi

if [ "x$LD_LIBRARY_PATH" == "x" ] ; then
  LD_LIBRARY_PATH=$SOS_ROOT/src/python
else
  LD_LIBRARY_PATH=$SOS_ROOT/src/python:$LD_LIBRARY_PATH
fi

export SOS_ENV_SET=1
