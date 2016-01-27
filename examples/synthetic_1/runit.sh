#!/bin/bash -e

rm -f new1.ppm *.bp *.trc *.edf *.slog2

i=100

A="-np 2 ../../bin/synthetic_worker_a ${i}"
B="-np 2 ../../bin/synthetic_worker_b ${i}"
C="-np 2 ../../bin/synthetic_worker_c ${i}"

mpirun ${A} &
sleep 1
mpirun ${B} &
sleep 1
mpirun ${C}

files=(tautrace.*.trc)

if [ -e "${files[0]}" ] ; then
    tau_treemerge.pl
    tau2slog2 tau.trc tau.edf -o tau.slog2
    rm *.trc *.edf
fi