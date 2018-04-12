
#include <stdio.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"

#include "sos_options.h"


int SOS_options_init(
        SOS_options   **sos_options_ptr_ref,
        SOS_role        role,
        char           *filepath,
        char           *special_settings_key)
{

    //Initialize the options
    *sos_options_ptr_ref = (SOS_options *) calloc(1, sizeof(SOS_options));
    SOS_options *opt = *sos_options_ptr_ref;

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

    // TODO: Process what is available...
    //
    //  -argv/argc
    //  -environment
    //  -actual file
  
    if (SOS_str_opt_is_enabled(getenv("SOS_BATCH_ENVIRONMENT"))) {
        opt->batch_environment = true;
    } else {
        opt->batch_environment = false;
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

    return 0;
}



void SOS_options_destroy(SOS_options *sos_options_ptr) {
    SOS_SET_CONTEXT(sos_options_ptr->sos_context, "SOS_options_destroy");



    return;
}
