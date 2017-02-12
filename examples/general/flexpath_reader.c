/* 
 * ADIOS is freely available under the terms of the BSD license described
 * in the COPYING file in the top level directory of this source distribution.
 *
 * Copyright (c) 2008 - 2009.  UT-BATTELLE, LLC. All rights reserved.
 */

/*************************************************************/
/*          Example of reading arrays in ADIOS               */
/*    which were written from the same number of processors  */
/*                                                           */
/*        Similar example is manual/2_adios_read.c           */
/*************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mpi.h"
#include "adios.h"
#include "adios_read.h"
#include "main.h"
#include <unistd.h>


typedef struct __adios_metadata__ {
    ADIOS_FILE* afile;
    ADIOS_VARINFO *nx_info;
    ADIOS_VARINFO *ny_info;
    ADIOS_VARINFO *nz_info;
    ADIOS_VARINFO *size_info;
    ADIOS_VARINFO *arry;
} ADIOS_METADATA;


void
slice(uint64_t length, uint64_t *s, uint64_t *e)
{
    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t rem = length % comm_size;

    start = length/comm_size * my_rank;
    end = length/comm_size * (my_rank+1);
    *s = start;
    *e = end;
    
    /* If our MPI size is greater
       than the number of y dimensions,
       then read the whole thing. */
    if (comm_size > length) {
        *e = length;
        *s = 0;
        return;\
    }
    if (end > length) {
        end = length;
        *e = end;
        return;
    }
    if (my_rank == comm_size-1) {
        end += rem;
        *e = end;
    }
}


int flexpath_reader (MPI_Comm adios_comm, int source_index) 
{
    int         j;
    int         NX, NY, NZ; 
    double      *t;

    // we have an adios "metadata" for each source we are reading from.
    static ADIOS_METADATA* _am = NULL;
    if (_am == NULL) {
        _am = (ADIOS_METADATA*)(calloc(num_sources,sizeof(ADIOS_METADATA)));
        adios_read_init_method(ADIOS_READ_METHOD_FLEXPATH, adios_comm, "verbose=3");
    }

    // get our "metadata" for this source
    ADIOS_METADATA* am = &(_am[source_index]);
    // has the source been opened?
    if (am->afile == NULL) {
        // open this source
        char file_name[256] = {0};
        sprintf(file_name, "adios_%s_%s", sources[source_index], my_name);
        double timeout_seconds = 1.0;
        // Enter this "loop" to wait for the file.
        my_printf("%s %d: Opening stream: %s\n", my_name, my_rank, file_name);
        am->afile = adios_read_open(file_name, ADIOS_READ_METHOD_FLEXPATH, 
                                    adios_comm, ADIOS_LOCKMODE_ALL, timeout_seconds);
        while (adios_errno == err_file_not_found) {
            my_printf("%s %d: Waiting on stream: %s\n", my_name, my_rank, adios_errmsg());
            sleep(1);
            adios_clear_error(); // reset the error
            my_printf("%s %d: Trying stream again: %s\n", my_name, my_rank, adios_errmsg());
            am->afile = adios_read_open(file_name, ADIOS_READ_METHOD_FLEXPATH, 
                                        adios_comm, ADIOS_LOCKMODE_ALL, timeout_seconds);
        }
        if(adios_errno == err_end_of_stream) {
            // stream is gone before we could open it.
            my_printf("%s %d: Stream terminated before open. %s\n", my_name, my_rank, adios_errmsg());
            return (1);
        } else if (am->afile == NULL) {
            // some other error happened
            my_printf("%s %d: Stream terminated before open. %s\n", my_name, my_rank, adios_errmsg());
            return (1);
        }
        /* get the array dimensions - only has to be done once */
        am->nx_info = adios_inq_var(am->afile, "/scalar/dim/NX");
        am->ny_info = adios_inq_var(am->afile, "/scalar/dim/NY");
        am->nz_info = adios_inq_var(am->afile, "/scalar/dim/NZ");
        am->size_info = adios_inq_var( am->afile, "size");
        am->arry = adios_inq_var( am->afile, "var_2d_array");
    } else {
        // advance to the next step of data
        adios_advance_step(am->afile, 0, 0);
        // handle errors
        switch(adios_errno) {
            if (adios_errno != err_no_error) {
                case err_no_error:
                    break;
                case err_end_of_stream:
                    my_printf("%s stream from %s has ended\n", my_name, sources[source_index]);
                    return(2);
                case err_step_notready:
                    my_printf("%s stream from %s is not ready\n", my_name, sources[source_index]);
                    return(2);
                case err_step_disappeared:
                    my_printf("%s stream from %s has disappeared!\n", my_name, sources[source_index]);
                    return(2);
                default:
                    fprintf (stderr, "%s stream from %s: Error %d at opening: %s\n", my_name, sources[source_index], adios_errno, adios_errmsg());
                    return(2);
            }
        }
    }

    ADIOS_SELECTION *global_range_select;
    ADIOS_SELECTION scalar_block_select;
    scalar_block_select.type = ADIOS_SELECTION_WRITEBLOCK;
    scalar_block_select.u.block.index = 0;

    /* schedule_read of a scalar. */    
    int shutdown = 0;

    int i;
    /* for(i=0; i<am->afile->nvars; i++){ */
    /*         printf("var: %s\n", am->afile->var_namelist[i]); */
    /* } */
    
    my_printf ("File info:\n");
    my_printf ("  current step:   %d\n", am->afile->current_step);
    my_printf ("  last step:      %d\n", am->afile->last_step);
    my_printf ("  # of variables: %d:\n", am->afile->nvars);
    int ii = am->afile->current_step;

        int nx_val = *((int*)am->nx_info->value);
        int ny_val = *((int*)am->ny_info->value);
        int size_val = *((int*)am->size_info->value);

        //printf("nx: %d, ny: %d, size: %d\n", nx_val, ny_val, size_val);
        
        // slice array along y dimension
        uint64_t my_ystart, my_yend, my_ycount;
        slice(am->arry->dims[1], &my_ystart, &my_yend);

        /* printf("rank: %d my_ystart: %d, my_yend: %d\n", */
        /*        my_rank, (int)my_ystart, (int)my_yend); */

        uint64_t xcount = am->arry->dims[0];
        uint64_t ycount = my_yend - my_ystart;
        uint64_t zcount = am->arry->dims[2];

        uint64_t starts[] = {0, my_ystart, 0};
        uint64_t counts[] = {xcount, ycount, zcount};

        /* printf("rank: %d starts: %d %d %d. counts: %d %d %d\n", */
        /*        my_rank, */
        /*        (int)starts[0], (int)starts[1], (int)starts[2], */
        /*        (int)counts[0], (int)counts[1], (int)counts[2]); */

        global_range_select = adios_selection_boundingbox(am->arry->ndim, starts, counts);

        int nelem = xcount*ycount*zcount;

        if(am->nx_info->value) {
            NX = *((int *)am->nx_info->value);
        }
        if(am->ny_info->value){
            NY= *((int*)am->ny_info->value);
        }
        if(am->nz_info->value){
            NZ= *((int*)am->nz_info->value);
        }
    
    /*
        if(my_rank == 0){
            int n;
            printf("dims: [ ");
            for(n=0; n<am->arry->ndim; n++){
                printf("%d ", (int)am->arry->dims[n]);
            }
            printf("]\n");
        }
    */
    
        /* Allocate space for the arrays */
        t = (double *)(calloc(nelem, sizeof(double)));
      
        /* Read the arrays */        
        adios_schedule_read (am->afile, 
                             global_range_select, 
                             "var_2d_array", 
                             0, 1, t);
        adios_schedule_read (am->afile,
                             &scalar_block_select,
                             "shutdown",
                             0, 1, &shutdown);

        adios_perform_reads (am->afile, 1);                
    
        //sleep(20);
    
        printf("%s Rank=%d: shutdown: %d step: %d\n", my_name, my_rank, shutdown, ii);
    /*
        printf("%s Rank=%d: shutdown: %d step: %d, t[0,5+x] = [", my_name, my_rank, shutdown, ii);
        for(j=0; j<nelem; j++) {
            printf(", %6.2f", t[j]);
        }
        printf("]\n\n");
        */
        //adios_release_step(am->afile);

    if(adios_errno == err_end_of_stream || shutdown > 0){
        printf("%s Rank=%d: closing file: %d step: %d\n", my_name, my_rank, shutdown, ii);
        adios_read_close(am->afile);
        return 1;
    }
    return 0;
}
