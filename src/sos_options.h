
#ifndef SOS_OPTIONS_H
#define SOS_OPTIONS_H

#include "sos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int SOS_options_init(
        void           *sos_context, 
        SOS_options   **sos_options_ptr_ref,
        char           *options_file,
        char           *options_class);

void SOS_options_destroy(SOS_options *sos_options_ptr);

void SOS_options_load_evar(SOS_options *opt);
void SOS_options_load_argv(SOS_options *opt);
void SOS_options_load_file(SOS_options *opt);


#ifdef __cplusplus
}
#endif

#endif
