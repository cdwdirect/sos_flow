#!/bin/bash -e

rm -f new1.ppm *.bp

i=100

A="-np 2 ../../bin/synthetic_worker_a ${i}"
B="-np 2 ../../bin/synthetic_worker_b ${i}"
C="-np 2 ../../bin/synthetic_worker_c ${i}"

mpirun ${A} &
sleep 1
mpirun ${B} &
sleep 1
mpirun ${C}