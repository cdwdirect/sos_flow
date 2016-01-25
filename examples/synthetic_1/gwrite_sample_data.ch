adios_groupsize = 4 \
                + 4 \
                + 4 \
                + 8 * (1) * (iterations);
adios_group_size (adios_handle, adios_groupsize, &adios_totalsize);
adios_write (adios_handle, "iterations", &iterations);
adios_write (adios_handle, "myrank", &myrank);
adios_write (adios_handle, "commsize", &commsize);
adios_write (adios_handle, "sample_val", sample_val);
