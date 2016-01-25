#!/bin/bash -e

A="-np 2 ../../bin/synthetic_worker_a 100"
B="-np 2 ../../bin/synthetic_worker_b"
C="-np 2 ../../bin/synthetic_worker_c"
mpirun ${C} &
mpirun ${B} &
mpirun ${A}