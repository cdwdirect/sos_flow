# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
BASEDIR="$(cd "$SCRIPTPATH/../.."; pwd)"
echo $BASEDIR

export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export PATH="${PATH}:$SOS_ROOT/scripts"
export BUILDDIR=build-linux
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export CC=gcc
export CXX=g++
export MPICC=mpicc
export MPICXX=mpicxx
export TAU_ARCH=x86_64
export TAU_CONFIG=-mpi-pthread
export TAU_ROOT=$HOME/src/tau2
export ADIOS_ROOT=/usr/local/packages/adios/git-gcc-4.9
if [ `hostname` = "ln1" ] ; then
  export CHAOS=$HOME/src/chaos/linux-gcc
  module use /projects/tau/packages/Modules/modulefiles
  module load python/2.7.13
else
  export CHAOS=/usr/local/packages/adios/chaos
fi

export cmake_extras_examples="-DADIOS_ROOT=${ADIOS_ROOT} -DSOS_ROOT=${BASEDIR}/${BUILDDIR}"

export PKG_CONFIG_PATH=${CHAOS}/lib/pkgconfig:${PKG_CONFIG_PATH}
export LD_LIBRARY_PATH=${CHAOS}/lib:${ADIOS_ROOT}/lib64:${LD_LIBRARY_PATH}
export PATH=${CHAOS}/bin:${ADIOS_ROOT}/bin:${PATH}

if [ `hostname` != "ln1" ] ; then
  module load cmake autoconf automake sqlite mpi-tor/openmpi-1.8_gcc-4.9 gcc/4.9 python
fi


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
