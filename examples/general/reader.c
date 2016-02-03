/* 
 * ADIOS is freely available under the terms of the BSD license described
 * in the COPYING file in the top level directory of this source distribution.
 *
 * Copyright (c) 2008 - 2009.  UT-BATTELLE, LLC. All rights reserved.
 */

/* ADIOS C Example: read global arrays from a BP file
 *
 * This code is using the generic read API, which can read in
 * arbitrary slices of an array and thus we can read in an array
 * on arbitrary number of processes (provided our code is smart 
 * enough to do the domain decomposition).
 *
 * Run this example after adios_global, which generates 
 * adios_global.bp. Run this example on equal or less 
 * number of processes since we decompose only on one 
 * dimension of the global array here. 
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mpi.h"
#include "adios_read.h"
#include "main.h"

int reader (MPI_Comm adios_comm, char* source)
{
    int         i, j, npl, token;
    MPI_Status  status;
    ADIOS_SELECTION * sel;
    void * data = NULL;
    uint64_t start[1], count[1];
    //enum ADIOS_READ_METHOD method = ADIOS_READ_METHOD_FLEXPATH;
    enum ADIOS_READ_METHOD method = ADIOS_READ_METHOD_BP;
    char file_name[256] = {0};
    sprintf(file_name, "adios_%s_%s.bp", source, my_name);

    ADIOS_FILE * f = adios_read_open (file_name, method, 
                                      adios_comm, ADIOS_LOCKMODE_NONE, 0);
    if (f == NULL)
    {
        printf ("%s\n", adios_errmsg());
        return -1;
    }

    ADIOS_VARINFO * v = adios_inq_var (f, "temperature");

    /* Using less readers to read the global array back, i.e., non-uniform */
    uint64_t slice_size = v->dims[0]/commsize;
    start[0] = slice_size * myrank;
    if (myrank == commsize-1) /* last rank may read more lines */
        slice_size = slice_size + v->dims[0]%commsize;
    count[0] = slice_size;

    data = malloc (slice_size * sizeof (double));
    if (data == NULL)
    {
        fprintf (stderr, "malloc failed.\n");
        return -1;
    }

    /* Read a subset of the temperature array */
    sel = adios_selection_boundingbox (v->ndim, start, count);
    adios_schedule_read (f, sel, "temperature", 0, 1, data);
    adios_perform_reads (f, 1);

    if (myrank > 0) {
        MPI_Recv (&token, 1, MPI_INT, myrank-1, 0, adios_comm, &status);
    }

    printf (" ======== Rank %d of %s from %s ========== \n", myrank, my_name, source);
    npl = 10;
    for (i = 0; i < slice_size; i+=npl) {
        printf ("[%4.4lld]  ", myrank*slice_size+i);
        for (j= 0; j < npl; j++) {
            printf (" %6.6g", * ((double *)data + i + j));
        }
        printf ("\n");
    }
    fflush(stdout);
    //sleep(1);

    if (myrank < commsize-1) {
        MPI_Send (&token, 1, MPI_INT, myrank+1, 0, adios_comm);
    }

    free (data);

    adios_read_close (f);
    MPI_Barrier (adios_comm);
    return 0;
}
