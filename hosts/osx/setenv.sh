# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
BASEDIR="$(cd "$SCRIPTPATH/../.."; pwd)"
echo $BASEDIR

export EVENT_NOKQUEUE=1

export SOS_HOST_KNOWN_AS="\"OS X (Generic)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""
export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export SOS_WORK=/tmp
export BUILDDIR=build-osx
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpicxx

export SOS_ENV_SET=1

echo "Reconfiguring the build scripts..."
cd $SOS_ROOT
$SOS_ROOT/scripts/configure.sh -c
cd $SOS_BUILD_DIR
make clean
echo "-- Compile SOSflow with the following command:"
echo ""
echo "        make -j install"
echo ""
