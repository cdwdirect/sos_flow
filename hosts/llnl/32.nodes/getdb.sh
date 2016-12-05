#!/bin/bash
echo ""
echo "Copying files from [$HOSTNAME:$SOS_WORK] to [$RESULT_ARCHIVE]  ..."
echo ""
mv $SOS_WORK/sosd.* $RESULT_ARCHIVE
sleep 10

