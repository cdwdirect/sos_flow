# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH="$(cd "$(dirname "${BASH_SOURCE}")"; pwd)"
BASEDIR=`dirname \`dirname ${SCRIPTPATH}\``
echo "SOS Base directory: ${BASEDIR}"

module load cmake
module swap PrgEnv-pgi PrgEnv-gnu
module load dataspaces

export SOS_CMD_PORT=22500
export SOS_DB_PORT=22503
export SOS_ROOT=${BASEDIR}
export SOS_WORK=.
export ACCOUNT=csc103
export BUILDDIR=${MEMBERWORK}/${ACCOUNT}/sos_flow/build-titan
export CC=gcc
export CXX=g++
export MPICC=cc
export MPICXX=CC
export TAU_ARCH=craycnl
export TAU_OPTIONS=-mpi-pthread
export TAU_ROOT=${HOME}/src/tau2
export ADIOS_ROOT=${HOME}/src/chaos/adios/1.11-gcc
export PATH=${ADIOS_ROOT}/bin:${PATH}

export cflags=`cc --cray-print-opts=cflags`
export libs=`cc --cray-print-opts=libs`
export cmake_extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_gnu_49_mt.so -DADIOS_ROOT=${ADIOS_ROOT} -DENABLE_ADIOS_EXAMPLES=TRUE -DFIX_ADIOS_DEPENDENCIES=TRUE"

export PKG_CONFIG_PATH=${HOME}/src/chaos/titan-gcc/lib/pkgconfig:${PKG_CONFIG_PATH}

export SOS_ENV_SET=1
