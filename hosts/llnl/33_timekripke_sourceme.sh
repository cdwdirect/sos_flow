#!/bin/bash


export SOS_JOB_TITLE="33.timekripke"

echo ""
echo "Killing all existing 'srun' invocations..."
killall -q srun
echo ""
sleep 2

####
#
#  Launch the SOS runtime:
#
#  Verify the environment has been configured:
source /usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/job_scripts/batchenv.sh
#
# Because we're running interactively...
export SOS_BATCH_ENVIRONMENT=""
#
#
#
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
#
# Example:   srun -N 1 -n 8 -r 1 $SOS_BUILD_DIR/bin/demo_app -i 1 -p 5 -m 25
#

# Copy the binary, configuration, and plotting scripts into the folder
# where the output of the job is being stored.


#export JOB_LAUNCH_COMMAND="-N 32 -n 512 -r 1 ./kripke_par --procs 8,8,8 --zones 512,512,512 --niter 20 --grp 1:1 --legendre 4 --quad 20:20 --dir 50:1"
#export JOB_LAUNCH_COMMAND="-N 32 -n 512 -r 1 ./kripke_par --procs 8,8,8 --zones 64,64,64 --niter 48 --grp 1:1 --legendre 4 --quad 20:20 --dir 50:1"
export JOB_LAUNCH_COMMAND="-N 32 -n 512 -r 1 ./kripke_par --procs 8,8,8 --zones 64,64,64 --niter 200 --grp 1:1 --legendre 4 --quad 20:20 --dir 50:1"


export JOB_BINARY_PATH=$PROJECT_BASE/sos_vpa_2017/matt/kripke
cp $JOB_BINARY_PATH/kripke_par $SOS_WORK
cp $JOB_BINARY_PATH/alpine_options.json $SOS_WORK
cp $JOB_BINARY_PATH/alpine_actions.json $SOS_WORK
cp $PROJECT_BASE/matts_python_stuff/ssos.py $SOS_WORK
cp $PROJECT_BASE/matts_python_stuff/ssos_python.o $SOS_WORK
cp $PROJECT_BASE/matts_python_stuff/ssos_python.so $SOS_WORK
cp $PROJECT_BASE/matts_python_stuff/plot_lulesh.py $SOS_WORK
cp $PROJECT_BASE/matts_python_stuff/visitlog.py $SOS_WORK
cp $PROJECT_BASE/matts_python_stuff/vtk_writer.py $SOS_WORK
#
# Make an archive of this script and the environment config script:
echo "srun $JOB_LAUNCH_COMMAND" > $SOS_WORK/LAUNCH_COMMAND
cp $PROJECT_BASE/job_scripts/batchenv.sh $SOS_WORK
cp $BASH_SOURCE $SOS_WORK/LAUNCH_SCRIPT
#

# Go into this location, so that VTK files created will be stored alongside
# the databases for archival/reproducibility purposes.
export CODEMORE=`pwd`
cd $SOS_WORK

# Run 8 processes on the 2nd node of the allocation.
# This leaves node 0 for pure aggregator / interactive vis
# work, the way things would be on a larger run...
#

#cp $PROJECT_BASE/job_scripts/time_clear.sh $SOS_WORK
#cp $PROJECT_BASE/job_scripts/time_log.sh $SOS_WORK
#cp $PROJECT_BASE/job_scripts/time_gather.sh $SOS_WORK

mkdir -p $SOS_WORK/timelogs
cp $PROJECT_BASE/job_scripts/scrubtime.py $SOS_WORK/timelogs/.

for TIME_ITER in $(seq 1 12)
do
    #mkdir -p "$SOS_WORK/timelogs/$TIME_ITER"
    # 
    #for THIS_NODE in $(seq 1 32)
    #do
    #    TIME_CLEAR_COMMAND="-N 1 -n 1 -r $THIS_NODE $SOS_WORK/time_clear.sh $THIS_NODE"
    #    srun $TIME_CLEAR_COMMAND &
    #done
    #echo "<<<< Waiting for all nodes to finish clearing their time logs.  (5 seconds)"
    #sleep 5
    #
    #####
    #####
    #####
    echo -n "$(date +%H:%M), $(date +%s), " > $SOS_WORK/timelogs/sos.$TIME_ITER 
    srun $JOB_LAUNCH_COMMAND
    echo "$(date +%H:%M), $(date +%s)" >> $SOS_WORK/timelogs/sos.$TIME_ITER

    echo "--------- SIMULATION $TIME_ITER of 12 COMPLETE ----------"
    echo "Pausing 30 seconds before next iteration..."
    sleep 30
    
    #####
    #####
    #####
    #
    #for THIS_NODE in $(seq 1 32)
    #do
    #    TIME_LOG_COMMAND="-N 1 -n 1 -r $THIS_NODE $SOS_WORK/time_log.sh $THIS_NODE"
    #    srun $TIME_LOG_COMMAND &
    #done
    #
    #echo "<<<< Waiting for all nodes to finish logging time.  (5 seconds)"
    #sleep 5
    #
    #for THIS_NODE in $(seq 1 32)
    #do
    #    TIME_GATHER_COMMAND="-N1 -n 1 -r $THIS_NODE $SOS_WORK/time_gather.sh $THIS_NODE $SOS_WORK/timelogs/$TIME_ITER/."
    #    srun $TIME_GATHER_COMMAND &
    #done
    #
    #  NOTE: Commented out b/c growing DB historically doesn't slow SOS down
    #        and messing w/DB in the filesystem like this when the daemon
    #        is persistent can cause issues with it.  No reason to do this.
    #
    #echo "<<<< Resetting SOS databases."
    #wipedb
    #echo "<<<< Sleeping for 20 seconds."
    #sleep 20
    #showdb
    #echo "<<<< Running another iteration in 5 seconds."
    #sleep 5
done

echo "<<<< Concatenating times file:"
echo -n "" > $SOS_WORK/timelogs/times.csv
for TIME_ITER in $(seq 1 12)
do
    echo "$TIME_ITER, $(cat $SOS_WORK/timelogs/sos.$TIME_ITER)" >> $SOS_WORK/timelogs/times.csv
done
echo "<<<< Timed output:"
cd timelogs
python scrubtime.py > $SOS_WORK/SIMULATION_TIMES_IN_SECONDS
cd $SOS_WORK
cat SIMULATION_TIMES_IN_SECONDS
#
#^
#^^
#^^^
#^^^
####
#
#  Bring the SOS runtime down cleanly:
#
echo "--------------------------------------------------------------------------------"
echo ""
echo "    DONE!"
echo ""
echo "\$SOS_WORK = $SOS_WORK"
echo "Sharing the power of the results in \$SOS_WORK..."
$PROJECT_BASE/share_power $SOS_WORK 
echo ""
echo "\$SOS_WORK directory listing:"
echo "" > PARTING_INSTRUCTIONS
ls -AblFs --color
echo "--------------------------------------------------------------------------------"
echo "The SOS RUNTIME IS STILL UP so you can interactively query / run VisIt." >> PARTING_INSTRUCTIONS
echo "" >> PARTING_INSTRUCTIONS 
echo "You are now in the \$SOS_WORK directory with your RESULTS and SCRIPTS!" >> PARTING_INSTRUCTIONS 
echo "                                       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" >> PARTING_INSTRUCTIONS 
echo "" >> PARTING_INSTRUCTIONS 
echo "        To RETURN to your code ..: $ cd \$CODEMORE" >> PARTING_INSTRUCTIONS 
echo "        To SHUT DOWN SOS ........: \$ srun -N 32 -n 32 -r 1 \$SOS_BUILD_DIR/bin/sosd_stop    (OR: \$ killall srun)" >> PARTING_INSTRUCTIONS 
echo "        To VISUALIZE results ....: \$ ./plot_lulesh.py" >> PARTING_INSTRUCTIONS 
echo "" >> PARTING_INSTRUCTIONS
cat PARTING_INSTRUCTIONS
#
####
