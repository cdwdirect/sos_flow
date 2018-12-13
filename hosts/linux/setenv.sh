# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
BASEDIR="$(cd "$SCRIPTPATH/../.."; pwd)"

export SOS_HOST_KNOWN_AS="\"Linux (Generic)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""

# Runtime options
export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export SOS_WORK=`pwd`
export BUILDDIR=build
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpicxx

export SOS_SHUTDOWN="FALSE"
export SOS_BATCH_ENVIRONMENT="FALSE"
#
export SOS_OPTIONS_FILE=""
export SOS_FWD_SHUTDOWN_TO_AGG="FALSE"
export SOS_EVPATH_MEETUP=$SOS_WORK
#
export SOS_DB_DISABLED="FALSE"
export SOS_UPDATE_LATEST_FRAME="TRUE"
export SOS_IN_MEMORY_DATABASE="FALSE"
export SOS_EXPORT_DB_AT_EXIT="FALSE"
#
export SOS_PUB_CACHE_DEPTH="0"
#
export SOS_SYSTEM_MONITOR_ENABLED="FALSE"
export SOS_SYSTEM_MONITOR_FREQ_USEC="0"

echo "======"
echo ""
echo "Script settings:"
echo "     \$SCRIPTPATH         = $SCRIPTPATH"
echo "     \$BASEDIR            = $BASEDIR"
echo ""
echo "Environment configured for SOSflow:"
echo "     \$SOS_HOST_KNOWN_AS  = $SOS_HOST_KNOWN_AS"
echo "     \$SOS_HOST_NODE_NAME = $SOS_HOST_NODE_NAME"
echo "     \$SOS_HOST_DETAILED  = $SOS_HOST_DETAILED"
echo "     \$SOS_CMD_PORT       = $SOS_CMD_PORT"
echo "     \$SOS_ROOT           = $SOS_ROOT"
echo "     \$SOS_WORK           = $SOS_WORK"
echo "     \$SOS_BUILD_DIR      = $SOS_BUILD_DIR"
echo ""
echo "SOSflow runtime options:" 
echo "     \$SOS_SHUTDOWN            = $SOS_SHUTDOWN"
echo "     \$SOS_OPTIONS_FILE        = $SOS_OPTIONS_FILE"
echo "     \$SOS_BATCH_ENVIRONMENT   = $SOS_BATCH_ENVIRONMENT"
echo "     \$SOS_FWD_SHUTDOWN_TO_AGG = $SOS_FWD_SHUTDOWN_TO_AGG"
echo ""
echo "     \$SOS_DB_DISABLED         = $SOS_DB_DISABLED"
echo "     \$SOS_IN_MEMORY_DATABASE  = $SOS_IN_MEMORY_DATABASE"
echo "     \$SOS_EXPORT_DB_AT_EXIT   = $SOS_EXPORT_DB_AT_EXIT"
echo "     \$SOS_UPDATE_LATEST_FRAME = $SOS_UPDATE_LATEST_FRAME"
echo "     \$SOS_EVPATH_MEETUP       = $SOS_EVPATH_MEETUP"
echo ""
echo "     \$SOS_SYSTEM_MONITOR_ENABLED   = $SOS_SYSTEM_MONITOR_ENABLED"
echo "     \$SOS_SYSTEM_MONITOR_FREQ_USEC = $SOS_SYSTEM_MONITOR_FREQ_USEC"
echo ""

echo "Updating paths:"
echo "     \$PATH            += \$SOS_ROOT/scripts"
export PATH="${PATH}:$SOS_ROOT/scripts"

echo "     \$PYTHONPATH      += \$SOS_ROOT/src/python"
if [ "x$PYTHONPATH" == "x" ] ; then
  PYTHONPATH=$SOS_ROOT/src/python
else
  PYTHONPATH=$SOS_ROOT/src/python:$PYTHONPATH
fi

echo "     \$LD_LIBRARY_PATH += \$SOS_BUILD_DIR/lib:\$SOS_ROOT/src/python"
if [ "x$LD_LIBRARY_PATH" == "x" ] ; then
  LD_LIBRARY_PATH=$SOS_ROOT/src/python
else
  LD_LIBRARY_PATH=$SOS_BUILD_DIR/lib:$SOS_ROOT/src/python:$LD_LIBRARY_PATH
fi

echo ""
SOSPIDLIST=`ps -C sosd --no-headers -o pid= | tr -s "\n" " "`
if [ "x$SOSPIDLIST" == "x" ] ; then
    echo "SOSflow is NOT currently running on this host."
else
    echo ">>>>> WARNING:                                              <<<<<"
    echo ">>>>> WARNING:  SOSflow IS CURRENTLY RUNNING on this host.  <<<<<"
    echo ">>>>> WARNING:                                              <<<<<"
    echo ""
    ps -C sosd 
    echo ""
    echo ">>>>> It is recommended that you restart SOSflow.           <<<<<"
    echo ">>>>> This command will terminate your SOSflow daemon[s]:   <<<<<"
    echo ""
    echo "           kill -9 $SOSPIDLIST"
    echo ""
fi

echo ""
echo "Done."
export SOS_ENV_SET=1

