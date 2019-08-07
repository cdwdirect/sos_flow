
#include <stdio.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_debug.h"
#include "sos_error.h"

#include "sos_options.h"


int SOS_options_init(
        void           *sos_context,
        SOS_options   **sos_options_ptr_ref,
        char           *options_file,
        char           *options_class)
{
    SOS_runtime *SOS_obj = sos_context;

    // NOTE: Options get loaded in three orders of priority:
    //           1. config file
    //           2. environment variables (overrides config file)
    //           3. command line params (overrides env.vars and config)

    //Initialize the options
    *sos_options_ptr_ref = (SOS_options *) calloc(1, sizeof(SOS_options));
    SOS_options *opt = *sos_options_ptr_ref;


    opt->sos_context    = sos_context;
    opt->role           = SOS_obj->role; 
    opt->options_file   = options_file;
    opt->options_class  = options_class;

    //Set default and/or 'sentinel' values.
    opt->listener_port      = -999;
    opt->listener_count     = -999;
    opt->aggregator_count   = -999;

    opt->build_dir          = NULL;
    opt->install_dir        = NULL;
    opt->source_dir         = NULL;
    opt->project_dir        = NULL;
    opt->work_dir           = NULL;
    opt->discovery_dir      = NULL;

    opt->db_disabled        = false;
    opt->db_in_memory_only  = false;
    opt->db_export_at_exit  = false; //Does nothing if db_disabled == true;
    opt->db_export_verbose  = false;
    opt->db_update_frame    = true;
    opt->db_frame_limit     = 0;     //0 == No limit

    opt->pub_cache_depth    = 0;     //0 == No value cacheing
    opt->batch_environment    = false;
    opt->udp_enabled          = false;
    opt->fwd_shutdown_to_agg  = false;

 
    opt->system_monitor_enabled   = false;
    opt->system_monitor_freq_usec = -1;

    // STEP 1/3: If provided, load options from a config file:
    if (opt->options_file != NULL) {
        if (strlen(opt->options_file) > 0) {
            SOS_options_load_file(opt);
        }
    }

    // STEP 2/3: Load any settings from environment variables:
    SOS_options_load_evar(opt);

    // STEP 3/3: If provided, load options from command line:
    if ((SOS_obj->config.argc > 2)
     && (SOS_obj->config.argv != NULL)) {
        SOS_options_load_argv(opt);
    }
    return 0;
}
void SOS_options_load_file(SOS_options *opt)
{
    if (!SOS_file_exists(opt->options_file)) {
        fprintf(stderr, "WARNING: Invalid path specified for"
                " SOS options file.  Skipping.\n            "
                " Path given: %s\n", opt->options_file);
        return;
    }

    // TODO: Load and scan options file.

    return;
}


void SOS_options_load_evar(SOS_options *opt) {
    // The default "baseline" for options is to use evars.

    if (SOS_str_opt_is_enabled(getenv("SOS_BATCH_ENVIRONMENT"))) {
        opt->batch_environment = true;
    } else {
        opt->batch_environment = false;
    }

    if (SOS_str_opt_is_enabled(getenv("SOS_FWD_SHUTDOWN_TO_AGG"))) {
        opt->fwd_shutdown_to_agg = true;
    } else {
        opt->fwd_shutdown_to_agg = false;
    }


    if (getenv("SOS_DISCOVERY_DIR") != NULL) {
        opt->discovery_dir = getenv("SOS_DISCOVERY_DIR");
    } else {
        opt->discovery_dir = NULL;
    }

    if (SOS_str_opt_is_enabled(getenv("SOS_IN_MEMORY_DATABASE"))) {
        opt->db_in_memory_only = true;
    } else {
        opt->db_in_memory_only = false;
    }

    if (SOS_str_opt_is_enabled(getenv("SOS_EXPORT_DB_AT_EXIT"))) {
        opt->db_export_at_exit = true;
        //Now test if we need to display output during export:
        if (strcmp("VERBOSE", getenv("SOS_EXPORT_DB_AT_EXIT")) == 0) {
            opt->db_export_verbose = true;
        } else {
            opt->db_export_verbose = false;
        }
    } else {
        opt->db_export_at_exit = false;
    }

    if (SOS_str_opt_is_disabled(getenv("SOS_UPDATE_LATEST_FRAME"))) {
        // In some cases we might not want to use any extra time
        // to synchronize the values stored in:
        //    tblPubs.latest_frame
        //    tblData.latest_frame
        opt->db_update_frame = false;
    } else {
        // This behavior defaults to being on unless explicitly disabled.
        opt->db_update_frame = true;
    }

    if (SOS_str_opt_is_enabled(getenv("SOS_DB_DISABLED"))) {
        opt->db_disabled = true;
        // Disabling the database overrides other database settings:
        opt->db_export_verbose = false;
        opt->db_export_at_exit = false;
        opt->db_in_memory_only = false;
        opt->db_update_frame   = false;
    } else {
        opt->db_disabled = false;
    }

    if (getenv("SOS_PUB_CACHE_DEPTH") != NULL) {
        opt->pub_cache_depth = atoi(getenv("SOS_PUB_CACHE_DEPTH"));
        if (opt->pub_cache_depth < 0) {
            opt->pub_cache_depth = 0;
        } else if (opt->pub_cache_depth > 100000) {
            fprintf(stderr, "WARNING: Unusually large value selected"
                    " for default pub->cache_depth! (%d)\n",
                    opt->pub_cache_depth);
        }
    } else {
        // No pub cache depth is selected.
        opt->pub_cache_depth = 0;
    }

    if (SOS_str_opt_is_enabled(getenv("SOS_UDP_ENABLED"))) {
        opt->udp_enabled = true;
    } else {
        opt->udp_enabled = false;
    }

    if (SOS_str_opt_is_enabled(getenv("SOS_SYSTEM_MONITOR_ENABLED"))) {
        opt->system_monitor_enabled   = true;
        char *usec_delay = getenv("SOS_SYSTEM_MONITOR_FREQ_USEC");
        if (usec_delay == NULL) {
            fprintf(stderr, "WARNING: System monitoring ENABLED but"
                    " SOS_SYSTEM_MONITOR_FREQ_USEC not provided."
                    " Defaulting to 1 second intervals.\n");
            fflush(stderr);
            opt->system_monitor_freq_usec = 1e6;
        } else {
            opt->system_monitor_freq_usec = atoi(usec_delay);
        }
    } else {
        opt->system_monitor_enabled   = false;
        opt->system_monitor_freq_usec = -1;
    }
    
    return;
}

void SOS_options_load_argv(SOS_options *opt)
{
    SOS_runtime *SOS_obj = opt->sos_context;
    int    argc = SOS_obj->config.argc;
    char **argv = SOS_obj->config.argv;

    if ((argc < 3) || (argv == NULL)) {
        // Nothing useful on the command line, skip.
        return;
    }

    return;
}

void SOS_options_destroy(SOS_options *sos_options_ptr) {
    SOS_SET_CONTEXT(sos_options_ptr->sos_context, "SOS_options_destroy");



    return;
}
