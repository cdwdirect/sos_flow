module unload darshan
module load cmake
module load sqlite 
module swap PrgEnv-intel PrgEnv-gnu
module load python/3.5-anaconda

export SOS_CMD_PORT=22500
export SOS_DB_PORT=22503
export SOS_ROOT=$HOME/src/sos_flow
export SOS_WORK=.

TAU=$HOME/src/tau2
#ADIOS=$HOME/install/adios/1.9.0_gcc-4.9
# ADIOS=$HOME/install/adios/1.9.0_gcc-4.9-tau
# CHAOS=$HOME/install/chaos/stable
# MXML=/usr/local/packages/mxml-2.7/gcc-4.5.3
# PATH=$HOME/install/gdb/7.9/bin:$ADIOS/bin:$TAU/x86_64/bin:$SOS_ROOT/bin:$PATH
# LD_LIBRARY_PATH=$CHAOS/lib:$ADIOS/lib:$MXML/lib:$LD_LIBRARY_PATH
# CPATH=$ADIOS/include:$CPATH

LD_LIBRARY_PATH=$TAU/craycnl/lib:$LD_LIBRARY_PATH
PATH=$TAU/craycnl/bin:$SQLITE_ROOT/bin:$PATH
export TAU_MAKEFILE=${TAU}/craycnl/lib/Makefile.tau-gnu-ompt-mpi-pdt-openmp
