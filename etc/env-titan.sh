
export SOS_CMD_PORT=22500
export SOS_DB_PORT=22503
export SOS_ROOT=$BASEDIR
export SOS_WORK=.
export BUILDDIR=build-titan
export CC=gcc
export CXX=g++
export MPICC=cc
export MPICXX=CC

# need to figure out how to use this
cflags=`cc --cray-print-opts=cflags`
libs=`cc --cray-print-opts=libs`

extras="-DMPI_C_INCLUDE_PATH=${CRAY_MPICH2_DIR}/include -DMPI_C_LIBRARIES=${CRAY_MPICH2_DIR}/lib/libmpich_gnu_49_mt.so"

module load cmake
module swap PrgEnv-pgi/5.2.82 PrgEnv-gnu
