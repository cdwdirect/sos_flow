
/*
 *   Serialization portions of this code for [un]packing structs/numerics
 *   inspired by Beej's guide:  http://beej.us/guide/
 *                --2015, Chad Wood
 */

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
//#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_buffer.h"
#include "sos_debug.h"

#ifdef SOSD_DAEMON_SRC
#include "sosd.h"
#endif


// Macros for packing floats and doubles.
#define SOS_buffer_pack754_16(f) (SOS_buffer_pack754((f), 16, 5))
#define SOS_buffer_pack754_32(f) (SOS_buffer_pack754((f), 32, 8))
#define SOS_buffer_pack754_64(f) (SOS_buffer_pack754((f), 64, 11))
#define SOS_buffer_unpack754_16(i) (SOS_buffer_unpack754((i), 16, 5))
#define SOS_buffer_unpack754_32(i) (SOS_buffer_unpack754((i), 32, 8))
#define SOS_buffer_unpack754_64(i) (SOS_buffer_unpack754((i), 64, 11))


void SOS_buffer_init(void *sos_context, SOS_buffer **buffer_obj) {
    SOS_buffer_init_sized_locking(sos_context, buffer_obj, SOS_DEFAULT_BUFFER_MAX, false);
    return;
}

void SOS_buffer_init_sized(void *sos_context, SOS_buffer **buffer_obj, int max_size) {
    SOS_buffer_init_sized_locking(sos_context, buffer_obj, max_size, false);
    return;
}


void SOS_buffer_init_sized_locking(void *sos_context, SOS_buffer **buffer_obj, int max_size, bool locking) {
    SOS_SET_CONTEXT((SOS_runtime *)sos_context, "SOS_buffer_init_sized_locking");
    SOS_buffer *buffer;

    dlog(5, "Creating buffer:\n");
    buffer = *buffer_obj = (SOS_buffer *) malloc(sizeof(SOS_buffer));
    memset(buffer, '\0', sizeof(SOS_buffer));
    buffer->sos_context = sos_context;
    buffer->max = max_size;
    buffer->len = 0;

    dlog(5, "   ... allocating storage space.\n");
    buffer->data = (unsigned char *) malloc(buffer->max * sizeof(unsigned char));

    if (buffer->data == NULL) {
        dlog(8, "ERROR: Unable to allocate buffer space.  Terminating.\n");
        exit(EXIT_FAILURE);
    }

    memset(buffer->data, 0, buffer->max);

    buffer->is_locking = locking;
    if (locking) {
        dlog(5, "   ... creating buffer->lock.\n");
        buffer->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        if (buffer->lock == NULL) {
            dlog(0, "ERROR: Unable to create buffer->lock!\n");
            exit(EXIT_FAILURE);
        } else {
            pthread_mutex_init(buffer->lock, NULL);
            dlog(5, "   ... done.\n");
        }
    }

#ifdef USE_MUNGE
    buffer->ref_cred = NULL;
#endif

#ifdef SOSD_DAEMON_SRC
    SOSD_countof(buffer_creates++);
    SOSD_countof(buffer_bytes_on_heap += buffer->max);
#endif

    dlog(5, "   ...done.\n");

    return;
}



void SOS_buffer_clone(SOS_buffer **dest, SOS_buffer *src) {
    SOS_SET_CONTEXT(src->sos_context, "SOS_buffer_clone");
    SOS_buffer *new;

    dlog(4, "Cloning src into dest buffer... ");
    SOS_buffer_init_sized_locking(src->sos_context, dest, (src->max + 1), src->is_locking);
    new = *dest;
    new->len = src->len;
    memcpy(new->data, src->data, src->len);
#ifdef USE_MUNGE
    if (src->ref_cred != NULL) {
        new->ref_cred = strdup(src->ref_cred);
    }
#endif
    dlog(4, "done.   (dest->len == %d)\n", new->len);
    return;
}


void SOS_buffer_lock(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_lock");

    if (buffer->is_locking) {
        dlog(4, "Locking buffer: ");
        pthread_mutex_lock(buffer->lock);
        dlog(4, "Done.\n");
    } else {
        dlog(1, "WARNING: You tried to lock a non-locking buffer!\n");
    }

    return;
}


void SOS_buffer_unlock(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_unlock");

    if (buffer->is_locking) {
        dlog(4, "Unlocking buffer: ");
        pthread_mutex_unlock(buffer->lock);
        dlog(4, "Done.\n");
    } else {
        dlog(1, "WARNING: You tried to unlock a non-locking buffer!\n");
    }

    return;
}


void SOS_buffer_destroy(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_destroy");

    if (buffer == NULL) {
        dlog(0, "ERROR: You called SOS_buffer_destroy() on a NULL buffer!  Terminating.\n");
        exit(EXIT_FAILURE);
    }

#ifdef SOSD_DAEMON_SRC
    SOSD_countof(buffer_destroys++);
    SOSD_countof(buffer_bytes_on_heap -= buffer->max);
#endif

    dlog(8, "Destroying buffer:\n");
    if (buffer->is_locking) {
        SOS_buffer_lock(buffer);
        dlog(8, "   ... destroying mutex.\n");
        pthread_mutex_destroy(buffer->lock);
        free(buffer->lock);
    }
    dlog(8, "   ... free'ing data\n");
    free(buffer->data);
    
#ifdef USE_MUNGE
    if (buffer->ref_cred != NULL) {
        free(buffer->ref_cred);
        buffer->ref_cred = NULL;
    }
#endif
    
    dlog(8, "   ... free'ing object\n")
    free(buffer);
    dlog(8, "   ... done.\n");
    return;
}


void SOS_buffer_wipe(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_wipe");

    dlog(8, "Wiping out buffer:\n");
    memset(buffer->data, '\0', buffer->max);
    buffer->len = 0;

#ifdef USE_MUNGE
    if (buffer->ref_cred != NULL) {
        free(buffer->ref_cred);
        buffer->ref_cred = NULL;
    }
#endif
    
    dlog(8, "   ... done.   (buffer->max == %d)\n", buffer->max);
    return;
}


void SOS_buffer_grow(SOS_buffer *buffer, size_t grow_amount, char *from_func) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_grow");

    buffer->max += grow_amount;
    buffer->data = (unsigned char *) realloc(buffer->data, buffer->max);

    if (buffer->data == NULL) {
        dlog(0, "ERROR: Unable to expand buffer!  (called by: %s)\n", from_func);
        dlog(0, "ERROR: Requested grow_amount == %zd\n", grow_amount);
        exit(EXIT_FAILURE);
    } else {

#ifdef SOSD_DAEMON_SRC
        SOSD_countof(buffer_bytes_on_heap += grow_amount);
#endif
        dlog(8, "   ... done.\n");
    }
    return;

}


void SOS_buffer_trim(SOS_buffer *buffer, size_t to_new_max) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_trim");

    if (buffer->max == to_new_max) return;

    int original_max = buffer->max;

    dlog(5, "Trimming buffer:\n");
    dlog(5, "   ... realloc()'ing from %d to %zd bytes.\n", buffer->max, to_new_max);
    buffer->data = (unsigned char *) realloc(buffer->data, to_new_max);
    if (buffer->data == NULL) {
        dlog(0, "ERROR: Unable to trim buffer!\n");
        exit(EXIT_FAILURE);
    } else {
        buffer->max = to_new_max;
#ifdef SOSD_DAEMON_SRC
        SOSD_countof(buffer_bytes_on_heap -= (original_max - to_new_max));
#endif
        dlog(5, "   ... done.\n");
    }
    return;
}

/*
** pack754() -- pack a floating point number into IEEE-754 format
*/ 

uint64_t SOS_buffer_pack754(long double f, unsigned bits, unsigned expbits)
{
    //----------
    double fnorm;
    int shift;
    long long sign, exp, significand;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (f == 0.0) return 0; // get this special case out of the way

    // check sign and begin normalization
    if (f < 0) { sign = 1; fnorm = -f; }
    else { sign = 0; fnorm = f; }

    // get the normalized form of f and track the exponent
    shift = 0;
    while(fnorm >= 2.0) { fnorm /= 2.0; shift++; }
    while(fnorm < 1.0) { fnorm *= 2.0; shift--; }
    fnorm = fnorm - 1.0;

    // calculate the binary form (non-float) of the significand data
    significand = fnorm * ((1LL<<significandbits) + 0.5f);

    // get the biased exponent
    exp = shift + ((1<<(expbits-1)) - 1); // shift + bias

    // return the final answer
    return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
}


/*
** SOS_buffer_unpack754() -- unpack a floating point number from IEEE-754 format
*/

double SOS_buffer_unpack754(uint64_t i, unsigned bits, unsigned expbits)
{
    //----------

    double result;
    long long shift;
    unsigned bias;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (i == 0) return 0.0;

    // pull the significand
    result = (i&((1LL<<significandbits)-1)); // mask
    result /= (1LL<<significandbits); // convert back to float
    result += 1.0f; // add the one back on

    // deal with the exponent
    bias = (1<<(expbits-1)) - 1;
    shift = ((i>>significandbits)&((1LL<<expbits)-1)) - bias;
    while(shift > 0) { result *= 2.0; shift--; }
    while(shift < 0) { result /= 2.0; shift++; }

    // sign it
    result *= (i>>(bits-1))&1? -1.0: 1.0;

    return result;
}


/*
** packi32() -- store a 32-bit int into a char buffer (like htonl())
*/
void SOS_buffer_packi32(unsigned char *buf, int i)
{
    *buf++ = i>>24; *buf++ = i>>16;
    *buf++ = i>>8;  *buf++ = i;
}


/*
** packi64() -- store a 64-bit int into a char buffer (like htonl())
*/ 
void SOS_buffer_packi64(unsigned char *buf, int64_t i)
{
    *buf++ = i>>56; *buf++ = i>>48;
    *buf++ = i>>40; *buf++ = i>>32;
    *buf++ = i>>24; *buf++ = i>>16;
    *buf++ = i>>8;  *buf++ = i;
}


/*
** packguid() -- store a GUID in a char buffer (like htonl())
*/ 
void SOS_buffer_packguid(unsigned char *buf, SOS_guid g)
{
    *buf++ = g>>56; *buf++ = g>>48;
    *buf++ = g>>40; *buf++ = g>>32;
    *buf++ = g>>24; *buf++ = g>>16;
    *buf++ = g>>8;  *buf++ = g;
}




/*
** unpacki32() -- unpack a 32-bit int from a char buffer (like ntohl())
*/ 
int SOS_buffer_unpacki32(unsigned char *buf)
{
    unsigned int i2 = ((unsigned int)buf[0]<<24) |
        ((unsigned int)buf[1]<<16) |
        ((unsigned int)buf[2]<<8)  |
        buf[3];
    int i;

    // change unsigned numbers to signed
    if (i2 <= 0x7fffffffu) { i = i2; }
    else { i = -1 - (long int)(0xffffffffu - i2); }

    return i;
}

/*
** unpacki64() -- unpack a 64-bit int from a char buffer (like ntohl())
*/

int64_t SOS_buffer_unpacki64(unsigned char *buf)
{
    unsigned long i2 = ((unsigned long)buf[0]<<56) |
        ((unsigned long)buf[1]<<48) |
        ((unsigned long)buf[2]<<40) |
        ((unsigned long)buf[3]<<32) |
        ((unsigned long)buf[4]<<24) |
        ((unsigned long)buf[5]<<16) |
        ((unsigned long)buf[6]<<8)  |
        buf[7];
    long i;

    // change unsigned numbers to signed
    if (i2 <= 0x7fffffffffffffffu) { i = i2; }
    else { i = -1 - (long)(0xffffffffffffffffu - i2); }

    return i;
}


/*
** unpackguid() -- unpack a 64-bit unsigned from a char buffer (like ntohl())
*/
SOS_guid SOS_buffer_unpackguid(unsigned char *buf)
{
    return ((SOS_guid)buf[0]<<56) |
        ((SOS_guid)buf[1]<<48) |
        ((SOS_guid)buf[2]<<40) |
        ((SOS_guid)buf[3]<<32) |
        ((SOS_guid)buf[4]<<24) |
        ((SOS_guid)buf[5]<<16) |
        ((SOS_guid)buf[6]<<8)  |
        buf[7];
}


/*
** unpacku64() -- unpack a 64-bit unsigned from a char buffer (like ntohl())
*/ 
uint64_t SOS_buffer_unpacku64(unsigned char *buf)
{
    return ((unsigned long long int)buf[0]<<56) |
        ((unsigned long long int)buf[1]<<48) |
        ((unsigned long long int)buf[2]<<40) |
        ((unsigned long long int)buf[3]<<32) |
        ((unsigned long long int)buf[4]<<24) |
        ((unsigned long long int)buf[5]<<16) |
        ((unsigned long long int)buf[6]<<8)  |
        buf[7];
}





/*
** SOS_buffer_pack() -- store data dictated by the format string in the buffer
**
*/

int SOS_buffer_pack(SOS_buffer *buffer, int *offset, char *format, ...) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_pack");

    va_list ap;

    unsigned char *buf = (buffer->data + *offset);

    int      i;           // 32-bit
    long     l;           // 64-bit
    double   d;           // double
    SOS_guid g;           // GUID (64-bit uint)
    char    *s;           // strings
    unsigned char   *b;   // bytes (raw data array, blob, etc)
    unsigned char    false_b = '\0';

    uint64_t fhold;

    int len;
    unsigned int datalen;

    int packed_bytes;  // how many bytes have been packed...

    dlog(8, "Packing the following format string: \"%s\"\n", format);
    /* Check if the offset is more than half the buffer. this is
     * VERY conservative, but we likely won't have to check when
     * packing strings, later. */
    if (*offset > ((buffer->max) >> 1)) {
        SOS_buffer_grow(buffer, *offset, SOS_WHOAMI);
        // just in case the buffer moved.
        buf = (buffer->data + *offset);
    }

    packed_bytes = 0;
    datalen = 0;

    va_start(ap, format);

    for(; *format != '\0'; format++) {

        //Auto-grow a buffer if needed.
        /*while ((*offset + packed_bytes) >= (buffer->max - SOS_DEFAULT_BUFFER_MIN)) {
            dlog(0, "Growing... (%d + %d) >= (%d - %d)\n", 
                 *offset,
                 packed_bytes, 
                 buffer->max,
                 SOS_DEFAULT_BUFFER_MIN);

            SOS_buffer_grow(buffer);
            }*/

        switch(*format) {
        case 'i': // 32-bit
            i = va_arg(ap, int);
            dlog(8, "  ... packing i @ %d:   %d   [32-bit]\n", packed_bytes, (int) i);
            SOS_buffer_packi32(buf, i);
            buf += 4;
            packed_bytes += 4;
            break;
        case 'l': // 64-bit
            l = va_arg(ap, long);
            dlog(8, "  ... packing l @ %d:   %ld   [64-bit]\n", packed_bytes, (long) l);
            SOS_buffer_packi64(buf, l);
            buf += 8;
            packed_bytes += 8;
            break;
        case 'd': // float-64
            d = va_arg(ap, double);
            dlog(8, "  ... packing d @ %d:   %lf   [64-bit float]\n", packed_bytes, (double) d);
            fhold = SOS_buffer_pack754_64(d); // convert to IEEE 754
            SOS_buffer_packi64(buf, fhold);
            buf += 8;
            packed_bytes += 8;
            break;
        case 'g': // 64-bit (SOSflow GUID, traditionally 64-bit uint)
            g = va_arg(ap, SOS_guid);
            dlog(8, "  ... packing g @ %d:   %" SOS_GUID_FMT "   [GUID]\n", packed_bytes, (SOS_guid) g);
            SOS_buffer_packguid(buf, g);
            buf += 8;
            packed_bytes += 8;
            break;
        case 's': // string
            s = va_arg(ap, char*);
            len = strlen(s);
            dlog(8, "  ... packing s @ %d:   \"%s\"   (%d bytes + 4)   [STRING]\n", packed_bytes, s, len);
            SOS_buffer_packi32(buf, len);
            buf += 4;
            packed_bytes += 4;
            memcpy(buf, s, len);
            buf += len;
            packed_bytes += len;
            break;
        case 'b': // bytes
            b = va_arg(ap, unsigned char*);
            len = datalen;
            if (len < 1) {
                dlog(1, "  ... WARNING: You're trying to pack SOS_VAL_TYPE_BYTES w/out specifying a count!\n");
                dlog(1, "  ... WARNING: Length is given inline before the 'b' format: \"ii##b#biisll###ggi\"... etc.\n");
                dlog(1, "  ... WARNING: To prevent crashes, a single empty character is being packed.\n");
                len = 1;
                b = &false_b;
            }
            dlog(8, "  ... packing b @ %d:   \"%s\"   (%d bytes + 4)\n", packed_bytes, s, len);
            SOS_buffer_packi32(buf, len);
            buf += 4;
            packed_bytes += 4;
            memcpy(buf, b, len);
            buf += len;
            packed_bytes += len;
            break;

        default:
            if (isdigit(*format)) { // track byte-data length
                datalen = datalen * 10 + (*format-'0');
            }
        }//switch
        if (!isdigit(*format)) datalen = 0;
    }//for

    va_end(ap);
    dlog(8, "  ... done\n");

    *offset     += packed_bytes;
    buffer->len  = (buffer->len > *offset) ? buffer->len : *offset;

    return packed_bytes;
}



int
SOS_buffer_pack_bytes(SOS_buffer *buffer, int *offset, int byte_count, void *source) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_pack_bytes");

    unsigned char   *buf = (buffer->data + *offset);
    int              packed_bytes = 0;
    unsigned char    false_b = '\0';


    if (byte_count < 1) {
        dlog(1, "  ... WARNING: You're trying to pack SOS_VAL_TYPE_BYTES"
                " w/invalid length (%d)!\n", byte_count);
        dlog(1, "  ... WARNING: To prevent crashes, a single empty character is being packed.\n");
        byte_count = 1;
        source = &false_b;
    }

    /* Check if the offset is more than half the buffer. this is
     * VERY conservative, but we likely won't have to check when
     * packing strings, later. */
    if (*offset > ((buffer->max) >> 1)) {
        SOS_buffer_grow(buffer, *offset, SOS_WHOAMI);
        // just in case the buffer moved.
        buf = (buffer->data + *offset);
    }

    while ((*offset + 4 + byte_count) > (buffer->max + 1)) {
        SOS_buffer_grow(buffer, (*offset + byte_count + 4 + 1), SOS_WHOAMI);
        // just in case the buffer moved.
        buf = (buffer->data + *offset);
    }

    dlog(8, "  ... packing bytes @ %d: ----- (%d bytes + 4)\n", packed_bytes, byte_count);

    SOS_buffer_packi32(buf, byte_count);
    buf += 4;
    packed_bytes += 4;
    memcpy(buf, (unsigned char *) source, byte_count);
    buf += byte_count;
    packed_bytes += byte_count;

    dlog(8, "   ... done\n");

    *offset += packed_bytes;

    buffer->len  = (buffer->len > *offset) ? buffer->len : *offset;

    return packed_bytes;

}





/*
** unpack() -- unpack data dictated by the format string into the buffer
**
**  This function returns the number of bytes in the buffer that were read by
**  following the instructions in the format string.
**
*/
int SOS_buffer_unpack(SOS_buffer *buffer, int *offset, char *format, ...) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_unpack");

    va_list ap;

    while (*offset >= buffer->max) {
        dlog(0, "WARNING: Attempting to read beyond the end of a buffer!\n");
        dlog(0, "WARNING:   buffer->max == %d, SOS_unpack() w/offset == %d\n", buffer->max, *offset);
        dlog(0, "WARNING: ...growing the buffer.\n");
        SOS_buffer_grow(buffer, buffer->max, SOS_WHOAMI);
    }
    unsigned char *buf = (buffer->data + *offset);

    int      *i;       // 32-bit
    long     *l;       // 64-bit
    SOS_guid *g;       // GUID (64-bit uint)
    double   *d;       // double (64-bit)
    char     *s;       // string
    unsigned char *b;  // bytes (raw data)

    uint64_t  fhold;

    unsigned int len, count;
    unsigned int maxlen;
    int packed_bytes;

    dlog(8, "Unpacking the following format string: \"%s\"\n", format);

    packed_bytes = 0;
    maxlen = 0;

    va_start(ap, format);

    for(; *format != '\0'; format++) {
        switch(*format) {
        case 'i': // 32-bit
            i = va_arg(ap, int*);
            *i = SOS_buffer_unpacki32(buf);
            dlog(8, "  ... unpacked i @ %d:   %d   [32-bit]\n", packed_bytes, *i);
            buf += 4;
            packed_bytes += 4;
            break;
        case 'l': // 64-bit
            l = va_arg(ap, long*);
            *l = SOS_buffer_unpacki64(buf);
            dlog(8, "  ... unpacked l @ %d:   %ld   [64-bit]\n", packed_bytes, *l);
            buf += 8;
            packed_bytes += 8;
            break;
        case 'g': // 64-bit (SOSflow GUID, traditionally 64-bit uint)
            g = va_arg(ap, SOS_guid*);
            *g = SOS_buffer_unpackguid(buf);
            dlog(8, "  ... unpacked g @ %d:   %" SOS_GUID_FMT "   [GUID]\n", packed_bytes, *g);
            buf += 8;
            packed_bytes += 8;
            break;
        case 'd': // float-64
            d = va_arg(ap, double*);
            fhold = SOS_buffer_unpacku64(buf);
            *d = SOS_buffer_unpack754_64(fhold);
            dlog(8, "  ... unpacked d @ %d:   %lf   [64-bit double]\n", packed_bytes, *d);
            buf += 8;
            packed_bytes += 8;
            break;
        case 's': // string
            s = va_arg(ap, char*);
            len = SOS_buffer_unpacki32(buf);
            buf += 4;
            packed_bytes += 4;
            if (maxlen > 0 && len > maxlen) count = maxlen - 1;
            else count = len;
            if (s == NULL) {
                dlog(0, "WARNING: Having to calloc() space for a string, NULL (char *) provided.   [STRING]\n");
                s = (char *) calloc((count + 1), sizeof(char));
            }
            if (count > 0) {
                memcpy(s, buf, count);
            }
            s[count] = '\0';
            dlog(8, "  ... unpacked s @ %d:   \"%s\"   (%d bytes + 4)   [STRING]\n", packed_bytes, s, len);
            buf += len;
            packed_bytes += len;
            break;
        case 'b': // bytes
            b = va_arg(ap, unsigned char*);
            len = SOS_buffer_unpacki32(buf);
            buf += 4;
            packed_bytes += 4;
            if (maxlen > 0 && len > maxlen) count = maxlen - 1;
            else count = len;
            if (b == NULL) {
                b = (unsigned char *) calloc((count + 1), sizeof(unsigned char));
            }
            memcpy(s, buf, count);
            dlog(8, "  ... unpacked b @ %d:   \"%s\"   (%d bytes + 4)\n", packed_bytes, b, len);
            buf += len;
            packed_bytes += len;
            break;

        default:
            if (isdigit(*format)) { // track max str len
                maxlen = maxlen * 10 + (*format-'0');
            }
        }//switch
        if (!isdigit(*format)) maxlen = 0;
    }

    va_end(ap);
    dlog(8, "  ... done\n");
    

    *offset += packed_bytes;
    return packed_bytes;
}


// NOTE: Shortcut routine, since this sort of thing is helpful all over SOS.
void SOS_buffer_unpack_safestr(SOS_buffer *buffer, int *offset, char **dest) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOS_buffer_unpack_string_safely");

    int tmp_offset = *offset;
    int str_length  = 0;

    SOS_buffer_unpack(buffer, &tmp_offset, "i", &str_length);
    
    if (*dest != NULL) { free(*dest); }
    *dest = calloc((1 + str_length), sizeof(unsigned char));

    SOS_buffer_unpack(buffer, offset, "s", *dest);

    return;
}



