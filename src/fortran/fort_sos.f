


#include "sos_types.h"



    /* --- "public" functions for users of SOS --- */

    SOS_runtime* SOS_init(int *argc, char ***argv, SOS_role role, SOS_layer layer);
    SOS_pub*     SOS_pub_create(SOS_runtime *sos_context, char *pub_title, SOS_nature nature);
    int          SOS_pack(SOS_pub *pub, const char *name, SOS_val_type pack_type, void *pack_val_var);
    int          SOS_event(SOS_pub *pub, const char *name, SOS_val_semantic semantic);
    void         SOS_announce(SOS_pub *pub);
    void         SOS_publish(SOS_pub *pub);
    int          SOS_sense_register(SOS_runtime *sos_context, char *handle, void (*your_callback)(void *your_data));
    void         SOS_sense_activate(SOS_runtime *sos_context, char *handle, SOS_layer layer, void *data, int data_length);
    void         SOS_finalize(SOS_runtime *sos_context);


