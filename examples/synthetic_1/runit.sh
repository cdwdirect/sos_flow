#!/bin/bash -e

export SOS_ROOT=$HOME/src/sos_flow

# cleanup
rm -f new1.ppm *.bp *.trc *.edf *.slog2

# start the SOS daemon
export SOS_CMD_PORT=22500
${SOS_ROOT}/src/mpi.start.2 &
sleep 1

# launch our workflow
i=100
A="-np 2 ${SOS_ROOT}/bin/synthetic_worker_a ${i}"
B="-np 2 ${SOS_ROOT}/bin/synthetic_worker_b ${i}"
C="-np 2 ${SOS_ROOT}/bin/synthetic_worker_c ${i}"

mpirun ${A} &
mpirun ${B} &
mpirun ${C}

# post-process TAU files
files=(tautrace.*.trc)
if [ -e "${files[0]}" ] ; then
    tau_treemerge.pl
    tau2slog2 tau.trc tau.edf -o tau.slog2
    rm *.trc *.edf
fi

# shut down the daemon. DAEMON GET OUT!
${SOS_ROOT}/bin/sos_stop
sleep 1
