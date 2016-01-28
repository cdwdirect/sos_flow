#!/bin/bash -x

export SOS_ROOT=$HOME/src/sos_flow
export SOS_CMD_PORT=22500
cwd=`pwd`

cd /tmp
if [ ! -f adios_config.xml ] ; then
    ln -s ${cwd}/adios_config.xml .
fi
if [ ! -f tau.conf ] ; then
    ln -s ${cwd}/tau.conf .
fi

# cleanup
rm -f new1.ppm *.bp *.trc *.edf *.slog2 *info.txt *ready.txt *.db *.log *.lock

# start the SOS daemon
${SOS_ROOT}/src/mpi.start.2 &
sleep 1

# launch our workflow
i=100
A="-np 2 ${SOS_ROOT}/bin/synthetic_worker_a ${i}"
B="-np 2 ${SOS_ROOT}/bin/synthetic_worker_b ${i}"
C="-np 2 ${SOS_ROOT}/bin/synthetic_worker_c ${i}"

mpirun ${A} &
sleep 1
mpirun ${B} &
sleep 1
mpirun ${C}

sleep 1
# post-process TAU files
files=(tautrace.*.trc)
if [ -e "${files[0]}" ] ; then
    tau_treemerge.pl
    tau2slog2 tau.trc tau.edf -o tau.slog2
    rm *.trc *.edf
fi

# shut down the daemon. DAEMON GET OUT!
${SOS_ROOT}/bin/sosd_stop
sleep 1
