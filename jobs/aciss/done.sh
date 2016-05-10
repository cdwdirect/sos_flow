#!/bin/bash
echo "$HOSTNAME : Running finalize script."
cp /dev/shm/sos_data/* /home12/cdw/research/sos_flow/jobs/aciss/data/
echo "$HOSTNAME : Done finalizing."