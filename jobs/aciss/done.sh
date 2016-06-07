#!/bin/bash
echo "$HOSTNAME : Running finalize script."
echo "$HOSTNAME : Bringing down sosd_probe's..."
killall sosd_probe
echo "$HOSTNAME : Moving data files to target directory: ${SOS_ROOT}/jobs/aciss/data/${PBS_JOBID}"
mkdir -p ${SOS_ROOT}/jobs/aciss/data/${PBS_JOBID}
cp -R ${SOS_WORK}/sos_data/${PBS_JOBID}/* ${SOS_ROOT}/jobs/aciss/data/${PBS_JOBID}
echo "$HOSTNAME : Done finalizing."