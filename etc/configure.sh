#!/bin/bash -e
set -x

# remember where we are
STARTDIR=`pwd`
# Absolute path to this script, e.g. /home/user/bin/foo.sh
SCRIPT=$(readlink -f "$0")
# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH=$(dirname "$SCRIPT")
echo $SCRIPTPATH
BASEDIR=$SCRIPTPATH/..
# this gets overwritten in the system-specific env file
BUILDDIR=build

# Parse the arguments
args=$(getopt -l "searchpath:" -o "cdht" -- "$@")
clean=0
debug=0
tau=0

eval set -- "${args}"

while [ $# -ge 1 ]; do
    case "$1" in
        --)
            # No more options left.
            break
            ;;
        -c)
            clean=1
            ;;
        -d)
            debug=1
            ;;
        -t)
            tau=1
            ;;
        -h)
            echo "$0 [-c]"
            echo "-c : Clean"
            echo "-d : Debug"
            echo "-t : Use TAU"
            exit 0
            ;;
    esac
    shift
done


host=`hostname`
if [[ "${host}" == "titan"* ]] ; then
	. $BASEDIR/etc/env-titan.sh
fi

if [ "x${SOS_ROOT}" == "x" ] ; then
  echo "Please set the SOS_ROOT environment variable."
  kill -INT $$
fi
if [ "x${SOS_WORK}" == "x" ] ; then
  echo "Please set the SOS_WORK environment variable."
  kill -INT $$
fi
if [ "x${SOS_CMD_PORT}" == "x" ] ; then
  echo "Please set the SOS_CMD_PORT environment variable."
  kill -INT $$
fi

cd $BASEDIR
if [ ${clean} -eq 1 ] ; then
	rm -rf $BUILDDIR
fi
mkdir $BUILDDIR
cd $BUILDDIR

buildtype=RelWithDebInfo
if [ ${debug} -eq 1 ] ; then
	buildtype=Debug
fi

tauopts=""
if [ ${tau} -eq 1 ] ; then
	tauopts="-DUSE_TAU=TRUE -DTAU_ROOT=$HOME/src/tau2 -DTAU_ARCH=craycnl -DTAU_OPTIONS=-mpi-pthread"
fi

cmake \
    -DCMAKE_BUILD_TYPE=${buildtype} \
    --prefix=./bin \
    -DCMAKE_C_COMPILER=$CC \
    -DCMAKE_CXX_COMPILER=$CXX \
    -DMPI_C_COMPILER=$MPICC \
    -DMPI_CXX_COMPILER=$MPICXX \
    $extras \
    ..
