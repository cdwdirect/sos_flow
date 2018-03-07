
#ifndef SOS_OPTIONS_H
#define SOS_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

int SOS_options_init(
        SOS_options   **sos_options_ptr_ref,
        SOS_role        role,
        char           *filepath,
        char           *special_settings_key);

void SOS_options_destroy(SOS_options *sos_options_ptr);




#ifdef __cplusplus
}
#endif

#endif
