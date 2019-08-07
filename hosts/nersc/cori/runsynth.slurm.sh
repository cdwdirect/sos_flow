#!/bin/bash -l
#SBATCH -p debug
#SBATCH -N 2
#SBATCH -A m1881
#SBATCH -t 0:30:00
#SBATCH -C haswell
#SBATCH --gres=craynetwork:4

# See http://www.nersc.gov/users/computational-systems/cori/running-jobs/example-batch-scripts/ for details
# run this in an interactive session, launced with:
#   salloc -N 2 -p debug -t 00:30:00 --gres=craynetwork:4 -C haswell

export MPICH_MAX_THREAD_SAFETY=multiple
source /project/projectdirs/m1881/khuck/sos_flow/sos_flow/hosts/nersc/cori/setenv.sh
export SOS_WORK=/project/projectdirs/m1881/khuck/sos_flow/sos_flow/working

if [ "x${SOS_ROOT}" == "x" ] ; then 
  echo "Please set the SOS_ROOT environment variable."
  kill -INT $$
fi
if [ "x${SOS_WORK}" == "x" ] ; then 
  echo "Please set the SOS_WORK environment variable."
  kill -INT $$
fi
if [ "x${SOS_CMD_PORT}" == "x" ] ; then 
  echo "Please set the SOS_CMD_PORT environment variable."
  kill -INT $$
fi
echo ""
echo ""
echo "*** Starting [sosd] daemon..."
echo ""
echo ""

cd ${SOS_WORK}

numprocs=2
numnodes=$(expr ${SLURM_NNODES} - 1)
echo "Num Nodes: ${numnodes}"
cores=$(expr ${numnodes} \* 32)
echo "Num Cores: ${cores}"
#numthreads=$(expr ${cores} / ${numprocs})
numthreads=1
echo "Num Threads per proc: ${numthreads}"

if [ -z $1 ]; then echo ""; echo "*** Removing any files left by previous sosd daemons..."; echo ""; fi;
${SOS_ROOT}/scripts/mpi.cleanall

if [ -z $1 ]; then echo ""; echo "*** Launching the sosd daemons..."; echo ""; fi;
# cmd="srun --wait 60 -n ${SLURM_NNODES} -N ${SLURM_NNODES} -c 4 --hint=multithread --gres=craynetwork:1 --mem=25600 -l --multi-prog ./srun.multi.conf.2nodes"
cmd="srun --wait 60 -n ${SLURM_NNODES} -N ${SLURM_NNODES} -c 4 --hint=multithread --gres=craynetwork:1 --mem=25600 -l ${SOS_ROOT}/build-cori/bin/sosd -l 1 -a 1 -w ${SOS_WORK}"
echo $cmd
$cmd &

sleep 30
echo ""
echo ""
export OMP_NUM_THREADS=${numthreads}
# launch our workflow
cmd="srun -n ${numprocs} -N ${numnodes} -c 2 --hint=multithread --gres=craynetwork:1 --mem=25600 -l $SOS_ROOT/build-cori-examples/bin/synthetic_worker_a 100"
echo "*** Running [demo_app]... ${cmd}"
$cmd &
cmd="srun -n ${numprocs} -N ${numnodes} -c 2 --hint=multithread --gres=craynetwork:1 --mem=25600 -l $SOS_ROOT/build-cori-examples/bin/synthetic_worker_b 100"
echo "*** Running [demo_app]... ${cmd}"
$cmd &
cmd="srun -n ${numprocs} -N ${numnodes} -c 2 --hint=multithread --gres=craynetwork:1 --mem=25600 -l $SOS_ROOT/build-cori-examples/bin/synthetic_worker_c 100"
echo "*** Running [demo_app]... ${cmd}"
$cmd
sleep 20
echo ""
echo ""
echo "*** Shutting down the daemons..."
echo ""
echo ""
cmd="srun --wait 60 -n ${numnodes} -N ${numnodes} --gres=craynetwork:1 --mem=25600  $SOS_ROOT/build-cori/bin/sosd_stop"
echo $cmd
$cmd
echo ""
echo ""
echo ""
echo "*** Pausing for I/O flush (sleep 2) and displaying [showdb] results..."
echo ""
echo ""
sleep 10
$SOS_ROOT/build-cori/bin/showdb
echo ""
