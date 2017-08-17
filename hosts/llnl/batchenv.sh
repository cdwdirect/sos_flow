echo ""
echo "batchenv.sh: Configuring your environment for the SOSflow runtime..."
echo "" 
source /usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/unsetenv.sh
source /usr/workspace/wsa/pavis/third_party/toss3_gcc-4.9.3/setenv.sh
export SOS_BATCH_ENVIRONMENT="batch"
echo ""
echo "Setting \$SOS_WORK for this job..."
unset SOS_WORK
if [ "x$SOS_JOB_TITLE" == "x" ]
then
    export SOS_WORK=$PROJECT_BASE/job_work/$SLURM_JOBID
else
    export SOS_WORK=$PROJECT_BASE/job_work/$SLURM_JOBID.$SOS_JOB_TITLE
fi
rm -f $SOS_WORK
mkdir -p $SOS_WORK
echo ""
env | grep SOS
echo ""
source /usr/tce/packages/dotkit/init.sh
use visit
echo "--------------------------------------------------------------------------------"
echo "Your environment should be configured to run SOSflow experiments!"
echo ""
