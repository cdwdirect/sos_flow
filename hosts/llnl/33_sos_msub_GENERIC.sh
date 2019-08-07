#!/bin/bash
#MSUB -V -j oe -N "SOSflow_33node"
#MSUB -l nodes=33
#MSUB -l walltime=00:15:00

####
#
#  Launch the SOS runtime:
#
#  Verify the environment has been configured:
source /usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/job_scripts/batchenv.sh
if [ "x$SOS_ENV_SET" == "x" ] ; then
	echo "Please set up your SOS environment first."
    kill -INT $$
fi
if ls $SOS_EVPATH_MEETUP/sosd.*.key 1> /dev/null 2>&1
then
    echo "WARNING: Aggregator KEY file[s] exist already.  Deleting them."
    rm -f $SOS_EVPATH_MEETUP/sosd.*.key
fi
if ls $SOS_WORK/sosd.*.db 1> /dev/null 2>&1
then
    echo "WARNING: SOSflow DATABASE file[s] exist already.  Deleting them."
    evp.cleanall
fi
#
echo ""
echo "Launching SOS daemons..."
echo ""
#
#
SOS_DAEMON_TOTAL="33"
#
#
srun -N 1 -n 1 -r 0 ${SOS_BUILD_DIR}/bin/sosd -k 0 -r aggregator -l 32 -a 1 -w ${SOS_WORK} & 
echo "   ... aggregator(0) srun submitted."
for LISTENER_RANK in $(seq 1 32)
do
    srun -N 1 -n 1 -r $LISTENER_RANK ${SOS_BUILD_DIR}/bin/sosd -k $LISTENER_RANK -r listener   -l 32 -a 1 -w ${SOS_WORK} &
    echo "   ... listener($LISTENER_RANK) srun submitted."
done
#
#
echo ""
echo "Pausing to ensure runtime is completely established..."
echo ""
SOS_DAEMONS_SPAWNED="0"
while [ $SOS_DAEMONS_SPAWNED -lt $SOS_DAEMON_TOTAL ]
do
    if ls $SOS_WORK/sosd.*.db 1> /dev/null 2>&1
    then
        SOS_DAEMONS_SPAWNED="$(ls -l $SOS_WORK/sosd.*.db | grep -v ^d | wc -l)"
    else
        SOS_DAEMONS_SPAWNED="0"
    fi

    if [ "x$SOS_BATCH_ENVIRONMENT" == "x" ]; then
        for STEP in $(seq 1 20)
        do
            echo -n "  ["
            for DOTS in $(seq 1 $STEP)
            do
                echo -n "#"
            done
            for SPACES in $(seq $STEP 19)
            do
                echo -n " "
            done
            echo -n "]  $SOS_DAEMONS_SPAWNED of $SOS_DAEMON_TOTAL daemons running..."
            echo -ne "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
            echo -ne "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
            sleep 0.1
        done
    fi
done
echo ""
echo ""
echo "SOS is ready for use!"
echo ""
echo "Launching experiment code..."
echo ""
#
#### -------------------------------------------------------------------------
#vvv
#vvv  --- INSERT YOUR EXPERIMENT CODE HERE ---
#vv
#v

# NOTE: Make sure to use the '-r 1' flag on your srun commands
#       if you want to avoid colocating applications with your
#       SOS runtime aggregation daemon, since it is usually
#       busier than regular listeners.

srun -N 32 -n 32 -r 1 $SOS_BUILD_DIR/bin/demo_app -i 1 -p 5 -m 25

#^
#^^
#^^^
#^^^
####
#
#  Bring the SOS runtime down cleanly:
#
echo ""
echo "Experiment code is complete.  Bringing down the SOS runtime..."
sleep 2
echo ""
srun -N 32 -n 32 -r 1 $SOS_BUILD_DIR/bin/sosd_stop
echo ""
echo "Runtime stop command has been sent for to all listeners."
echo ""
#
#
echo ""
echo ""
SOS_DAEMONS_RUNNING="33"
while [ $SOS_DAEMONS_RUNNING -gt "0" ]
do
    if ls $SOS_WORK/sosd.*.lock 1> /dev/null 2>&1
    then
        SOS_DAEMONS_RUNNING="$(ls -l $SOS_WORK/sosd.*.lock | grep -v ^d | wc -l)"
    else
        SOS_DAEMONS_RUNNING="0"
    fi
    if [ "x$SOS_BATCH_ENVIRONMENT" == "x" ]; then
        for STEP in $(seq 1 20)
        do
            echo -n "  ["
            for DOTS in $(seq 1 $STEP)
            do
                echo -n "#"
            done
            for SPACES in $(seq $STEP 19)
            do
                echo -n " "
            done
            echo -n "]  $SOS_DAEMONS_RUNNING daemons still running..."
            echo -ne "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
            echo -ne "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
            sleep 0.1
        done
    fi
done
echo ""
echo "Done."
echo ""
#
####
