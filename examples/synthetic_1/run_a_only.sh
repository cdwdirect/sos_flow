#!/bin/bash -e

export SOS_ROOT=$HOME/src/sos_flow
export SOS_CMD_PORT=22500
cwd=`pwd`
working=/tmp/sos_flow_working

mkdir -p ${working}
cd ${working}
if [ ! -f adios_config.xml ] ; then
    ln -s ${cwd}/adios_config.xml .
fi
if [ ! -f tau.conf ] ; then
    ln -s ${cwd}/tau.conf .
fi

# cleanup
rm -rf new1.ppm *.bp *.trc *.edf *.slog2 *info.txt *ready.txt *.db *.log *.lock

# start the SOS daemon

if [ -z $1 ]; then echo "   >>> BATCH MODE!"; fi;
if [ -z $1 ]; then echo "   >>> Starting the sosd daemons..."; fi;
${SOS_ROOT}/src/mpi.cleanall
if [ -z $1 ]; then echo "   >>> Launching the sosd daemons..."; fi;
daemon0="-np 1 ${SOS_ROOT}/bin/sosd --role SOS_ROLE_DAEMON --port 22500 --buffer_len 8388608 --listen_backlog 10 --work_dir ${working}"
daemon1="-np 1 ${SOS_ROOT}/bin/sosd --role SOS_ROLE_DB     --port 22503 --buffer_len 8388608 --listen_backlog 10 --work_dir ${working}"
echo ${daemon0}
echo ${daemon1}

mpirun ${daemon0} : ${daemon1} &
sleep 1

# launch our workflow
i=20
A="-np 2 ${SOS_ROOT}/bin/synthetic_worker_a ${i} 0"

 #mpirun ${A}
mpirun -np 1 gdb --args ${SOS_ROOT}/bin/synthetic_worker_a ${i} 0

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
