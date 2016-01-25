s = adios_selection_writeblock (rank);
adios_schedule_read (fp, s, "sample_val", 0, 1, sample_val);
adios_perform_reads (fp, 1);
adios_selection_delete (s);
