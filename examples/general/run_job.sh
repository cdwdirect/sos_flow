#!/bin/bash -e
#PBS -q short
#PBS -l nodes=5:ppn=12
#PBS -l walltime=00:30:00

source /usr/local/packages/Modules/current/init/bash

cd $PBS_O_WORKDIR
source ../../sourceme.sh
cwd=$PBS_O_WORKDIR
working=${cwd}/sos_flow_working
# working=/tmp/sos_flow_working

export SOS_ROOT=$HOME/src/sos_flow
export SOS_CMD_PORT=22500

mkdir -p ${working}
cd ${working}

# cleanup
rm -rf new1.ppm *.bp *.trc *.edf *.slog2 *info.txt *ready.txt *.db *.log *.lock profile.*

if [ ! -f tau.conf ] ; then
    ln -s ${cwd}/tau.conf .
fi

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

${SOS_ROOT}/examples/general/general.py ${SOS_ROOT}/examples/general/5apps-multinode.json
post_process_tau
