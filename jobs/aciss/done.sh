#!/bin/bash
echo "$HOSTNAME : Running finalize script."
cp ${SOS_WORK}/sos_data/* ${SOS_ROOT}/jobs/aciss/data
echo "$HOSTNAME : Done finalizing."