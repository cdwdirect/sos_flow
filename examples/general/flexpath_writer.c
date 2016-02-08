/* 
 * ADIOS is freely available under the terms of the BSD license described
 * in the COPYING file in the top level directory of this source distribution.
 *
 * Copyright (c) 2008 - 2009.  UT-BATTELLE, LLC. All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include "mpi.h"
#include "adios.h"
#include "main.h"

/*************************************************************/
/*          Example of writing arrays in ADIOS               */
/*                                                           */
/*        Similar example is manual/2_adios_write.c          */
/*************************************************************/
int flexpath_writer (MPI_Comm adios_comm, char * sink) 
{
    int         rank, size, i, j, offset, size_y;
    int         NX = 5; 
    int         NY = 2;
    int         NZ = 2;
    double      t[NX*NY*NZ];
    MPI_Comm    comm = adios_comm;

    int64_t     adios_handle;
    rank = myrank;
    size = commsize;

    char file_name[256] = {0};
    sprintf(file_name, "adios_%s_%s", my_name, sink);
    adios_init ("arrays.xml", comm);
    
    int total = NX * NY * NZ * size;
    int test_scalar = rank * 1000;

    offset = rank*NY;
    size_y = size*NY;

    int start = 0;
    int myslice = NX * NY * NZ;

    for (i = 0; i<5; i++) {       
	for (j=0; j<NY*NX*NZ; j++) {       
	    t[j] = rank*myslice + (start + j);
	}

	int s;
	for (s = 0; s<size; s++) {
	    if (s == rank) {
		fprintf(stderr, "%s rank %d: step: %d [", my_name, rank, i);
		int z;
		for(z=0; z<NX*NY*NZ;z++){
		    fprintf(stderr, "%lf, ", t[z]);
		}
		fprintf(stderr, "]\n");
	    }
	    fprintf(stderr, "\n");
	    fflush(stderr);
	    //MPI_Barrier(MPI_COMM_WORLD);
	}

	start += total;
        //prints the array.
	adios_open (&adios_handle, "temperature", file_name, "w", comm);
	
	adios_write (adios_handle, "/scalar/dim/NX", &NX);
	adios_write (adios_handle, "/scalar/dim/NY", &NY);
	adios_write (adios_handle, "/scalar/dim/NZ", &NZ);
	adios_write (adios_handle, "test_scalar", &test_scalar);
	adios_write (adios_handle, "size", &size);
	adios_write (adios_handle, "rank", &rank);
	adios_write (adios_handle, "offset", &offset);
	adios_write (adios_handle, "size_y", &size_y);
	adios_write (adios_handle, "var_2d_array", t);
	
	adios_close (adios_handle);
    }

    adios_finalize (rank);
    return 0;
}
