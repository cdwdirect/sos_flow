#!/bin/bash 
set -x

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
echo "*** Terminating any prior daemons using [sosd_stop]..."
echo ""
echo ""
${SOS_ROOT}/${BUILDDIR}/bin/sosd_stop
echo ""
echo ""
echo "*** Starting [sosd] daemon..."
echo ""
echo ""
${SOS_ROOT}/scripts/mpi.cleanall
#ddt ${SOS_ROOT}/bin/sosd --role SOS_ROLE_DAEMON --port 22500  --work_dir ${SOS_WORK} &
#xterm -hold -e valgrind --max-stackframe=4728552 ${SOS_ROOT}/bin/sosd --role SOS_ROLE_DAEMON --port 22500 --work_dir ${SOS_WORK} &
#xterm -fa 'Monospace' -fs 18 -hold -e gdb --args ${SOS_ROOT}/bin/sosd --role SOS_ROLE_DAEMON --port 22500 --work_dir ${SOS_WORK} &
cmd="aprun -n 1 ${SOS_ROOT}/${BUILDDIR}/bin/sosd --role SOS_ROLE_DAEMON --port 22500 --work_dir ${SOS_WORK}"
echo $cmd
$cmd &
echo ""
echo ""
echo "*** Running [demo_app]..."
echo aprun ${SOS_ROOT}/${BUILDDIR}/bin/demo_app -i 200 -m 20000 -j 1.5


