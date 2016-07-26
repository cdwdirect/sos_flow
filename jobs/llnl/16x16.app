###################################################################
# srun multiple program configuration file
#
# srun -n8 -l --multi-prog myrun.conf
###################################################################
# 4-6       hostname
# 1,7       echo  task:%t
# 0,2-3     echo  offset:%o

0-119 /g/g17/wood67/src/sos_flow/bin/demo_sweep -imin 10 -imax 10 -istep 10 -smin 10 -smax 10 -sstep 10 -dmin 500000 -dmax 1000000 -dstep 500000
