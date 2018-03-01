
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

    //Set default 'sentinel' values.
    opt->listener_port    = -999;
    opt->listener_count   = -999;
    opt->aggregator_count = -999;
    opt->build_dir        = NULL;
    opt->install_dir      = NULL;
    opt->source_dir       = NULL;
    opt->project_dir      = NULL;
    opt->work_dir         = NULL;
    opt->discovery_dir    = NULL;

    // TODO: Process what is available...
    //
    //  -argv/argc
    //  -environment
    //  -actual file


    return 0;
}

void SOS_options_destroy(SOS_options *sos_options_ptr) {
    SOS_SET_CONTEXT(sos_options_ptr->sos_context, "SOS_options_destroy");
    
    return;
}
