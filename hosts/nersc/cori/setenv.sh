# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
# Old way (relative pathing):  BASEDIR=$SCRIPTPATH/../../..
BASEDIR="$(cd "$SCRIPTPATH/../../.."; pwd)"
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
export TAU_OPTIONS=-intel-mpi-pthread-pdt
export TAU_ROOT=/project/projectdirs/m1881/khuck/sos_flow/tau_contrib/tau2
export CHAOS=/project/projectdirs/m1881/khuck/sos_flow/chaos/cori-icc
export ADIOS_ROOT=/project/projectdirs/m1881/khuck/sos_flow/chaos/adios/1.11-icc
export PATH=${ADIOS_ROOT}/bin:${PATH}

# need to figure out how to use this
export cflags=`cc --cray-print-opts=cflags`
export libs=`cc --cray-print-opts=libs`
export cmake_extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_intel_mt.so"
export cmake_extras_examples="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_intel_mt.so -DADIOS_ROOT=${ADIOS_ROOT} -DFIX_ADIOS_DEPENDENCIES=TRUE -DSOS_ROOT=${BASEDIR}/${BUILDDIR}"
export ADIOS_WRAPPER_FLAGS=${ADIOS_ROOT}/adios_wrapper/link_options.tau

export PKG_CONFIG_PATH=${CHAOS}/lib/pkgconfig:${PKG_CONFIG_PATH}
export LD_LIBRARY_PATH=${CHAOS}/lib:${ADIOS_ROOT}/lib64:${LD_LIBRARY_PATH}
export PATH=${CHAOS}/bin:${ADIOS_ROOT}/bin:${PATH}

module unload darshan
module load cmake
module load sqlite 
#module swap PrgEnv-intel PrgEnv-gnu

export SOS_ENV_SET=1
export sos_env_set=1

TAU=/project/projectdirs/m1881/khuck/sos_flow/tau_contrib/tau2
PATH=$ADIOS/bin:$TAU/craycnl/bin:$PATH
LD_LIBRARY_PATH=$TAU/craycnl/lib:$LD_LIBRARY_PATH
# ADIOS=$HOME/install/adios/1.9.0_gcc-4.9
# ADIOS=$HOME/install/adios/1.9.0_gcc-4.9-tau
# MXML=/usr/local/packages/mxml-2.7/gcc-4.5.3
# PATH=$HOME/install/gdb/7.9/bin:$ADIOS/bin:$TAU/x86_64/bin:$SOS_ROOT/bin:$PATH
# LD_LIBRARY_PATH=$CHAOS/lib:$ADIOS/lib:$MXML/lib:$LD_LIBRARY_PATH
# CPATH=$ADIOS/include:$CPATH

# LD_LIBRARY_PATH=$TAU/craycnl/lib:$LD_LIBRARY_PATH
# PATH=$TAU/craycnl/bin:$SQLITE_ROOT/bin:$PATH
# export TAU_MAKEFILE=${TAU}/craycnl/lib/Makefile.tau-gnu-mpi-pthread-pdt
# export TAU_MAKEFILE=${TAU}/craycnl/lib/Makefile.tau-gnu-ompt-mpi-pdt-openmp
