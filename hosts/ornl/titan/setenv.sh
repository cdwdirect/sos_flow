# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
# Old way (relative pathing):   BASEDIR=$SCRIPTPATH/../..
BASEDIR="$(cd "$SCRIPTPATH/../../.."; pwd)"
echo $BASEDIR

module load cmake
module swap PrgEnv-pgi PrgEnv-gnu

export SOS_HOST_KNOWN_AS="\"ORNL (Titan)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""
export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export BUILDDIR=build-titan
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export CC=cc
export CXX=CC
export MPICC=cc
export MPICXX=CC
export TAU_ARCH=craycnl
export TAU_CONFIG=-gnu-mpi-pthread
export TAU_ROOT=/lustre/atlas/proj-shared/csc103/khuck/tau2
export ADIOS_ROOT=$HOME/src/chaos/adios/ADIOS-gcc
#export CHAOS=$HOME/src/chaos/titan-gcc
export CHAOS=/ccs/home/eisen/titan-gnu
#export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$CHAOS/lib/pkgconfig
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/ccs/home/khuck/src/sos_flow/hosts/ornl/titan/pkgconfig

# need to figure out how to use this
export cflags=`cc --cray-print-opts=cflags`
export libs=`cc --cray-print-opts=libs`
export cmake_extras="-DMPI_Fortran_COMPILER=ftn -DMPI_C_COMPILER=cc -DMPI_CXX_COMPILER=CC"
#export cmake_extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_gnu_49.so"
#export cmake_extras_examples="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_gnu_49.so -DADIOS_ROOT=${ADIOS_ROOT} -DSOS_ROOT=${BASEDIR}/${BUILDDIR}"
export cmake_extras_examples="-DADIOS_ROOT=${ADIOS_ROOT} -DSOS_ROOT=${BASEDIR}/${BUILDDIR} -DCMAKE_SKIP_BUILD_RPATH=TRUE -DCMAKE_BUILD_WITH_INSTALL_RPATH=FALSE -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=FALSE"

export SOS_ENV_SET=1
export sos_env_set=1
