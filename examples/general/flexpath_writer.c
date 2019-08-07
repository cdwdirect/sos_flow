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
int flexpath_writer (MPI_Comm adios_comm, int sink_index, bool append, bool shutdown_flag) 
{
    int         i, j, offset, size_y;
    int         NX = 5; 
    int         NY = 2;
    int         NZ = 2;
    double      t[NX*NY*NZ];

    // this is our array of handles that we are writing to.
    static int64_t * adios_handles = 0;
    // this is our "current" handle, for convenience.
    int64_t * adios_handle = 0;

    // how much total data is there to transfer in the array?
    int total = NX * NY * NZ * comm_size;
    // this flag tells the workflow to shutdown.
    int shutdown = shutdown_flag ? 1 : 0;

    // offsets into the array for each MPI rank.
    offset = my_rank*NY;
    size_y = comm_size*NY;

    // Each MPI rank only writes part of the array.
    int myslice = NX * NY * NZ;
	for (j=0; j<NY*NX*NZ; j++) {       
	    t[j] = my_rank*myslice + (j);
	}

    // if we haven't allocated space for handles, do it.
    if (adios_handles == 0) {
        adios_handles = (int64_t*)(calloc(num_sinks, sizeof(int64_t)));
    }
    // if this file isn't open, open it.
    adios_handle = &(adios_handles[sink_index]);
    //if((*adios_handle) == 0) {
        char file_name[256] = {0};
        char group_name[256] = {0};
        sprintf(file_name, "adios_%s_%s", my_name, sinks[sink_index]);
        sprintf(group_name, "%s_to_%s", my_name, sinks[sink_index]);
        //printf("Opening %s for write\n", file_name); fflush(stdout);
        if(append) {
	        adios_open (adios_handle, group_name, file_name, "a", adios_comm);
        } else {
	        adios_open (adios_handle, group_name, file_name, "w", adios_comm);
        }
    //}

    /*
     * Write our variables.
     */
    uint64_t  adios_totalsize;
    uint64_t  adios_groupsize = 4 + 4 + 4 + 4 + 4 + 4 + 4 + 4 + 8 * (NZ) * (NY) * (NX);
    adios_group_size ((*adios_handle), adios_groupsize, &adios_totalsize);
	adios_write ((*adios_handle), "/scalar/dim/NX", &NX);
	adios_write ((*adios_handle), "/scalar/dim/NY", &NY);
	adios_write ((*adios_handle), "/scalar/dim/NZ", &NZ);
	adios_write ((*adios_handle), "shutdown", &shutdown);
	adios_write ((*adios_handle), "size", &comm_size);
	adios_write ((*adios_handle), "rank", &my_rank);
	adios_write ((*adios_handle), "offset", &offset);
	adios_write ((*adios_handle), "size_y", &size_y);
	adios_write ((*adios_handle), "var_2d_array", t);

    //if (shutdown_flag) {
        //printf("Closing %s for write\n", file_name); fflush(stdout);
	    adios_close (*adios_handle);
        adios_handle = 0;
    //}

    return 0;
}
