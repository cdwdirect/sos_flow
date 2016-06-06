#!/bin/bash
echo "$HOSTNAME : Running init script..."
rm -rf ${SOS_WORK}/sos_data
mkdir ${SOS_WORK}/sos_data
echo "$HOSTNAME : Running finalize script..."