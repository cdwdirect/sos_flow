# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
BASEDIR=$SCRIPTPATH/../..
echo $BASEDIR

export SOS_CMD_PORT=22500
export SOS_DB_PORT=22503
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export BUILDDIR=build-cori
export CC=icc
export CXX=icpc
export MPICC=cc
export MPICXX=CC
export TAU_ARCH=craycnl
export TAU_OPTIONS=-mpi-pthread
export TAU_ROOT=$HOME/src/tau2
export ADIOS_ROOT=/project/projectdirs/m1881/khuck/sos_flow/chaos/adios/1.11-icc
export PATH=${ADIOS_ROOT}/bin:${PATH}


# need to figure out how to use this
export cflags=`cc --cray-print-opts=cflags`
export libs=`cc --cray-print-opts=libs`
export cmake_extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_intel_mt.so -DADIOS_ROOT=${ADIOS_ROOT} -DENABLE_ADIOS_EXAMPLES=TRUE -DFIX_ADIOS_DEPENDENCIES=TRUE"

export PKG_CONFIG_PATH=/project/projectdirs/m1881/khuck/sos_flow/chaos/cori-icc/lib/pkgconfig:${PKG_CONFIG_PATH}

module unload darshan
module load cmake
module load sqlite 
#module swap PrgEnv-intel PrgEnv-gnu

export sos_env_set=1

# TAU=$HOME/src/tau2
# ADIOS=$HOME/install/adios/1.9.0_gcc-4.9
# ADIOS=$HOME/install/adios/1.9.0_gcc-4.9-tau
# CHAOS=$HOME/install/chaos/stable
# MXML=/usr/local/packages/mxml-2.7/gcc-4.5.3
# PATH=$HOME/install/gdb/7.9/bin:$ADIOS/bin:$TAU/x86_64/bin:$SOS_ROOT/bin:$PATH
# LD_LIBRARY_PATH=$CHAOS/lib:$ADIOS/lib:$MXML/lib:$LD_LIBRARY_PATH
# CPATH=$ADIOS/include:$CPATH

# LD_LIBRARY_PATH=$TAU/craycnl/lib:$LD_LIBRARY_PATH
# PATH=$TAU/craycnl/bin:$SQLITE_ROOT/bin:$PATH
# export TAU_MAKEFILE=${TAU}/craycnl/lib/Makefile.tau-gnu-mpi-pthread-pdt
# export TAU_MAKEFILE=${TAU}/craycnl/lib/Makefile.tau-gnu-ompt-mpi-pdt-openmp
