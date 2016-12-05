#!/bin/bash
#----------------------------------------------------
# Example SLURM job script to run multiple mpi
# applications within one batch job on TACC's
# Stampede system.
#----------------------------------------------------
#SBATCH -J multiple_mpi_job     # Job name
#SBATCH -o multiple_mpi_job.o%j # Name of stdout output file(%j expands to jobId)
#SBATCH -e multiple_mpi_job.o%j # Name of stderr output file(%j expands to jobId)
#SBATCH -p development          # Submit to the 'normal' or 'development' queue
#SBATCH -N 4                    # Total number of nodes requested (16 cores/node)
#SBATCH -n 64                   # Total number of mpi tasks requested
#SBATCH -t 01:30:00             # Run time (hh:mm:ss) - 1.5 hours
# The next line is required if the user has more than one project
# #SBATCH -A A-yourproject      # Allocation name to charge job against

# This example will run 3 MPI applications using 32 tasks,
# 16 tasks, and 16 tasks

#DO NOT use tacc_affinity with multiple MPI applications
# within the same batch script!
# If running in a hybrid mode, please contact the help desk
# for support.

# Launch each MPI application using the "-o" and "-n" flags
# in the background
#Application 1
ibrun -o 0 -n 32 ./my_mypi_1.exe &

#Application 2
ibrun -o 32 -n 16 ./my_mypi_2.exe &

#Application 2
ibrun -o 48 -n 16 ./my_mypi_3.exe &

#Wait for all the MPI applications to finish
wait