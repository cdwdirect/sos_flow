#!/bin/bash
echo "$HOSTNAME : Running init script..."
cd /dev/shm
rm -rf sos_data
mkdir sos_data
echo "$HOSTNAME : Running finalize script..."