
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_debug.h"

void
SOS_string_init(SOS_string **str_obj_addr)
{
    // TODO : SOS_string_init
    return;
}

void
SOS_string_destroy(SOS_string *str_obj)
{
    // TODO : SOS_string_destroy
    return;
}

bool
SOS_string_exists(SOS_string *str_obj)
{
    // TODO : SOS_string_exists
    return false;
}


uint32_t
SOS_string_crc32(SOS_string *str_obj)
{
    // NOTE: This function may be called during init/set/get
    //       so it doesn't check any existing value or
    //       store the crc32 value it computes in the
    //       object.  That is left to the calling function.
    
    if (!SOS_string_exists(str_obj)) {
        return 0;
    }
    
    int i;
    uint32_t byte, crc, mask;
    char *msg = str_obj->val;

    i = 0;
    crc = 0xFFFFFFFF;
    while (msg[i] != 0) {
        byte = msg[i];
        crc = crc ^ byte;
        // Loop unrolled eight times...
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        mask = -(crc & 1);
        crc = (crc >> 1) ^ (0xEDB88320 & mask);
        //
        i = i + 1;
    }

    return ~crc;
}

int
SOS_string_len(SOS_string *str_obj)
{
    // TODO : SOS_string_len
    return -1;
}

char *
SOS_string_val(SOS_string *str_obj)
{
    // TODO : SOS_string_val
    return NULL;
}

void
SOS_string_set(SOS_string *dest, const char *src)
{
    // TODO : SOS_string_set
    return;
}

void
SOS_string_get(char *dest, SOS_string *src)
{
    // TODO : SOS_string_get
    return;
}

void
SOS_string_setn(SOS_string *dest, int max_rd_len, const char *src)
{
    // TODO: SOS_string_setn
    return;
}

void
SOS_string_getn(char *dest, int max_wr_len, SOS_string *src)
{
    // TODO: SOS_string_getn
    return;
}

void
SOS_string_clone(SOS_string **dest, SOS_string *src)
{
    // TODO: SOS_string_clone
    return;
}

