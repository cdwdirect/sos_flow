#!/bin/bash -e

rm -f new1.ppm *.bp

A="-np 2 ../../bin/synthetic_worker_a 100"
B="-np 1 ../../bin/synthetic_worker_b"
C="-np 2 ../../bin/synthetic_worker_c"

mpirun ${A} &
sleep 1
mpirun ${B}
# mpirun ${C} &