# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "$BASH_SOURCE")"; pwd)"
echo $SCRIPTPATH
# Old way (relative pathing):  BASEDIR=$SCRIPTPATH/../../..
BASEDIR="$(cd "$SCRIPTPATH/../../.."; pwd)"
echo $BASEDIR

module unload darshan
module load cmake
module swap PrgEnv-intel PrgEnv-gnu
module load sqlite 

export SOS_HOST_KNOWN_AS="\"NERSC (Cori)\""

# For tracking the environment that SOS is built in:
export SOS_HOST_NODE_NAME="\"$(uname -n)\""
export SOS_HOST_DETAILED="\"$(uname -o) $(uname -r) $(uname -m)\""
export SOS_CMD_PORT=22500
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export BUILDDIR=build-cori
export SOS_BUILD_DIR=$BASEDIR/$BUILDDIR
export CC=`which cc`
export CXX=`which CC`
export FC=`which ftn`
export MPICC=`which cc`
export MPICXX=`which CC`
export MPIF90=`which ftn`
export CFLAGS=-dynamic
export CXXFLAGS=-dynamic
export FFLAGS=-dynamic
export TAU_ARCH=craycnl
export TAU_CONFIG=-intel-mpi-pthread-pdt
export TAU_ROOT=/project/projectdirs/m1881/khuck/sos_flow/tau_contrib/tau2
export CHAOS=/project/projectdirs/m1881/khuck/sos_flow/chaos/cori-icc
export ADIOS_ROOT=/project/projectdirs/m1881/khuck/sos_flow/chaos/adios/git-icc
export PATH=${ADIOS_ROOT}/bin:${PATH}
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$CHAOS/lib/pkgconfig
#export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/ccs/home/khuck/src/sos_flow/hosts/ornl/titan/pkgconfig

# need to figure out how to use this
export cflags=`cc --cray-print-opts=cflags`
export libs=`cc --cray-print-opts=libs`
export cmake_extras="-DMPI_Fortran_COMPILER=${FC} -DMPI_C_COMPILER=${CC} -DMPI_CXX_COMPILER=${CXX}"
#export cmake_extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_intel_mt.so"
#export cmake_extras_examples="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_intel_mt.so -DADIOS_ROOT=${ADIOS_ROOT} -DFIX_ADIOS_DEPENDENCIES=TRUE -DSOS_ROOT=${BASEDIR}/${BUILDDIR}"
export cmake_extras_examples="-DADIOS_ROOT=${ADIOS_ROOT} -DSOS_ROOT=${BASEDIR}/${BUILDDIR} -DCMAKE_SKIP_BUILD_RPATH=TRUE -DCMAKE_BUILD_WITH_INSTALL_RPATH=FALSE -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=FALSE"
# export ADIOS_WRAPPER_FLAGS=${ADIOS_ROOT}/adios_wrapper/link_options.tau

export PKG_CONFIG_PATH=${CHAOS}/lib/pkgconfig:${PKG_CONFIG_PATH}
export LD_LIBRARY_PATH=${CHAOS}/lib:${ADIOS_ROOT}/lib64:${LD_LIBRARY_PATH}
export PATH=${CHAOS}/bin:${ADIOS_ROOT}/bin:${PATH}

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

#echo "Reconfiguring the build scripts..."
#cd $SOS_ROOT
#$SOS_ROOT/scripts/configure.sh -c
#cd $SOS_BUILD_DIR
#make clean
#echo "-- Compile SOSflow with the following command:"
#echo ""
#echo "        make -j install"
#echo ""

