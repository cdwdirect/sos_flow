#!/bin/bash
echo "$HOSTNAME : Running init script..."
rm -rf ${SOS_WORK}/sos_data/${PBS_JOBID}
mkdir -p ${SOS_WORK}/sos_data/${PBS_JOBID}
echo "$HOSTNAME : Done running init script..."