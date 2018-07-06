#ifndef SOS_STRING_H
#define SOS_STRING_H


#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "sos.h"
#include "sos_types.h"

/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

    // NOTE: The character buffer of an SOS_string 'object' is
    //       immutable once set.  It can be set again to something
    //       new, but the internal (char *) value is to be
    //       treated as read-only.
    //
    //       If SOS debugging is enabled, strings will be automatically
    //       CRC-32 validated every time these functions are called.

    void        SOS_string_init(SOS_string **str_obj_addr);
    void        SOS_string_destroy(SOS_string *str_obj);

    bool        SOS_string_exists(SOS_string *str_obj);
    uint32_t    SOS_string_crc32(SOS_string *str_obj);

    int         SOS_string_len(SOS_string *str_obj);
    char       *SOS_string_val(SOS_string *str_obj);

    void        SOS_string_set(SOS_string *dest, const char *src);
    void        SOS_string_get(char *dest,       SOS_string *src);
               // Safer variants:
    void        SOS_string_setn(SOS_string *dest, int rd_len, const char *src);
    void        SOS_string_getn(char *dest,       int wr_len, SOS_string *src);

    void        SOS_string_clone(SOS_string **dest_obj_addr, SOS_string *src);

#ifdef __cplusplus
}
#endif

#endif
