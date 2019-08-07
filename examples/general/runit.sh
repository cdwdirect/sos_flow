#!/bin/bash -e

# just in case
killall -9 mpirun || true

export SOS_ROOT=$HOME/src/sos_flow
export SOS_CMD_PORT=22500
cwd=`pwd`
working=/tmp/sos_flow_working

mkdir -p ${working}
cd ${working}

# cleanup
rm -rf new1.ppm *.bp *.trc *.edf *.slog2 *info.txt *ready.txt *.db *.log *.lock profile.*

if [ ! -f arrays.xml ] ; then
    ln -s ${cwd}/*.xml .
fi
if [ ! -f tau.conf ] ; then
    ln -s ${cwd}/tau.conf .
fi

start_sos_daemon()
{
    # start the SOS daemon

    if [ -z $1 ]; then echo "   >>> BATCH MODE!"; fi;
    if [ -z $1 ]; then echo "   >>> Starting the sosd daemons..."; fi;
    ${SOS_ROOT}/src/mpi.cleanall
    if [ -z $1 ]; then echo "   >>> Launching the sosd daemons..."; fi;
    daemon0="-np 1 ${SOS_ROOT}/bin/sosd --role SOS_ROLE_DAEMON --port 22500 --buffer_len 8388608 --listen_backlog 
    10 --work_dir ${working}"
    daemon1="-np 1 ${SOS_ROOT}/bin/sosd --role SOS_ROLE_DB     --port 22503 --buffer_len 8388608 --listen_backlog 
    10 --work_dir ${working}"
    echo ${daemon0}
    echo ${daemon1}
    mpirun ${daemon0} : ${daemon1} &
    sleep 1
}

stop_sos_daemon()
{
    # shut down the daemon.
    ${SOS_ROOT}/bin/sosd_stop
    sleep 1
}

launch_workflow()
{
    # launch our workflow
    i=10
    w=2
    s=0.1
    A="-np ${w} ${SOS_ROOT}/bin/generic_node --name A --iterations ${i} --writeto B --writeto D"
    B="-np ${w} ${SOS_ROOT}/bin/generic_node --name B --iterations ${i} --readfrom A --writeto C"
    C="-np ${w} ${SOS_ROOT}/bin/generic_node --name C --iterations ${i} --readfrom B --writeto E"
    D="-np ${w} ${SOS_ROOT}/bin/generic_node --name D --iterations ${i} --readfrom A --writeto E"
    E="-np ${w} ${SOS_ROOT}/bin/generic_node --name E --iterations ${i} --readfrom C --readfrom D"

    mpirun ${A} &
    sleep ${s}
    mpirun ${B} &
    sleep ${s}
    mpirun ${C} &
    sleep ${s}
    mpirun ${D} &
    sleep ${s}
    mpirun ${E}
}

launch_debug_workflow()
{
    i=10
    w=1
    #A="-np ${w} ${SOS_ROOT}/bin/generic_node --name A --iterations ${i} --writeto B --writeto C"
    A="-np ${w} ${SOS_ROOT}/bin/generic_node --name A --iterations ${i} --writeto B"
    B="-np ${w} ${SOS_ROOT}/bin/generic_node --name B --iterations ${i} --readfrom A"
    #C="-np ${w} ${SOS_ROOT}/bin/generic_node --name C --readfrom A"
    mpirun ${A} &
    sleep 1
    mpirun ${B}
    #mpirun ${B} &
    #sleep 1
    #mpirun ${C}
}

post_process_tau()
{
    sleep 3
    # post-process TAU files
    files=(tautrace.*.trc)
    if [ -e "${files[0]}" ] ; then
        tau_treemerge.pl
        tau2slog2 tau.trc tau.edf -o ${cwd}/tau.slog2
        rm *.trc *.edf
    fi
    files=(profile.*.*.*)
    if [ -e "${files[0]}" ] ; then
        paraprof --pack ${cwd}/profile.ppk
        rm profile.*.*.*
    fi
}

start_sos_daemon
launch_workflow
post_process_tau
stop_sos_daemon
