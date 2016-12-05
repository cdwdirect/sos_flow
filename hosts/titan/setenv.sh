# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
BASEDIR=$SCRIPTPATH/../..
echo $BASEDIR

export SOS_CMD_PORT=22500
export SOS_DB_PORT=22503
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export BUILDDIR=build-titan
export CC=gcc
export CXX=g++
export MPICC=cc
export MPICXX=CC
export TAU_ARCH=craycnl
export TAU_OPTIONS=-mpi-pthread
export TAU_ROOT=$HOME/src/tau2

# need to figure out how to use this
export cflags=`cc --cray-print-opts=cflags`
export libs=`cc --cray-print-opts=libs`
export cmake_extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_gnu_49_mt.so"

module load cmake
module swap PrgEnv-pgi PrgEnv-gnu

export sos_env_set=1
