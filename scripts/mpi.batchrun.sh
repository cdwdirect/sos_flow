#!/bin/bash 
#COUNTER=0
#while [  $COUNTER -lt 1000 ]; do
#    echo The counter is $COUNTER
#    killall mpirun && ./mpi.clean && ./runit
#    sqlite3 sosd.0.db "select max(rowid) from tblvals;" >> validate.txt
#    let COUNTER=COUNTER+1 
#done
for i in {1..1000}; do ./runit; killall mpirun; done
