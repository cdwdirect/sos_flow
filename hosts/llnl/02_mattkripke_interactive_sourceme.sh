#!/bin/bash

export SOS_JOB_TITLE="02.kripke"

echo ""
echo "Killing all existing 'srun' invocations..."
killall -q srun
echo ""

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
SOS_DAEMON_TOTAL="2"
#
#
srun -N 1 -n 1 -r 0 ${SOS_BUILD_DIR}/bin/sosd -k 0 -r aggregator -l 1 -a 1 -w ${SOS_WORK} & 
echo "   ... aggregator(0) srun submitted."
srun -N 1 -n 1 -r 1 ${SOS_BUILD_DIR}/bin/sosd -k 1 -r listener   -l 1 -a 1 -w ${SOS_WORK} &
echo "   ... listener(1) srun submitted."
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
export JOB_LAUNCH_COMMAND="-N 1 -n 8 -r 1 ./kripke_par --procs 2,2,2 --zones 32,32,32 --niter 3 --dir 1:2 --grp 1:1 --legendre 4 --quad 4:4"
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
# Make an archive of this script and the environment configu script:
echo "srun $JOB_LAUNCH_COMMAND" > $SOS_WORK/LAUNCH_COMMAND
cp $PROJECT_BASE/job_scripts/batchenv.sh $SOS_WORK
cp $BASH_SOURCE $SOS_WORK/LAUNCH_SCRIPT
#

# Go into this location, so that VTK files created will be stored alongside
# the databases for archival/reproducibility purposes.
export CODEMORE=`pwd`
cd $SOS_WORK

# Run 8 kripke processes on the 2nd node of the allocation.
# This leaves node 0 for pure aggregator / interactive vis
# work, the way things would be on a larger run...
#
srun $JOB_LAUNCH_COMMAND
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
echo "        To SHUT DOWN SOS ........: \$ srun -N 1 -n 1 -r 1 \$SOS_BUILD_DIR/bin/sosd_stop    (OR: \$ killall srun)" >> PARTING_INSTRUCTIONS 
echo "        To VISUALIZE results ....: \$ ./plot_lulesh.py" >> PARTING_INSTRUCTIONS 
echo "" >> PARTING_INSTRUCTIONS
cat PARTING_INSTRUCTIONS
#
####
