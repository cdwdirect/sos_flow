


/*
 *   Serialization code for [un]packing structs/numerics in character arrays.
 *     ... lifted from Beej's guide:  http://beej.us/guide/
 *     ... modified to prepend 'SOS_buffer_' to the function signatures.
 *     ... see end of file for an example of use.
 *                --2015, Chad Wood
 */


#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "sos.h"
#include "sos_debug.h"

// macros for packing floats and doubles:
#define SOS_buffer_pack754_16(f) (SOS_buffer_pack754((f), 16, 5))
#define SOS_buffer_pack754_32(f) (SOS_buffer_pack754((f), 32, 8))
#define SOS_buffer_pack754_64(f) (SOS_buffer_pack754((f), 64, 11))
#define SOS_buffer_unpack754_16(i) (SOS_buffer_unpack754((i), 16, 5))
#define SOS_buffer_unpack754_32(i) (SOS_buffer_unpack754((i), 32, 8))
#define SOS_buffer_unpack754_64(i) (SOS_buffer_unpack754((i), 64, 11))


/*
 *  Old stuff...
 *
 *

uint64_t OLD_SOS_buffer_pack754(long double f, unsigned int bits, unsigned int expbits) {
    long double fnorm;
    int shift;
    long sign, exp, significand;
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
    significand = fnorm * ((1L<<significandbits) + 0.5f);
    
    // get the biased exponent
    exp = shift + ((1<<(expbits-1)) - 1); // shift + bias
    
    // return the final answer
    return (sign<<(bits-1)) | (exp<<(bits-expbits-1)) | significand;
}



long double OLD_SOS_buffer_unpack754(uint64_t i, unsigned bits, unsigned expbits) {
    long double result;
    long long shift;
    unsigned bias;
    unsigned significandbits = bits - expbits - 1; // -1 for sign bit

    if (i == 0) return 0.0;

    // pull the significand
    result = (i&((1L<<significandbits)-1)); // mask
    result /= (1L<<significandbits); // convert back to float
    result += 1.0f; // add the one back on

    // deal with the exponent
    bias = (1<<(expbits-1)) - 1;
    shift = ((i>>significandbits)&((1L<<expbits)-1)) - bias;
    while(shift > 0) { result *= 2.0; shift--; }
    while(shift < 0) { result /= 2.0; shift++; }

    // sign it
    result *= (i>>(bits-1))&1? -1.0: 1.0;

    return result;
}
*/




/*
** pack754() -- pack a floating point number into IEEE-754 format
*/ 

uint64_t SOS_buffer_pack754(double f, unsigned bits, unsigned expbits)
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
void SOS_buffer_packi32(unsigned char *buf, unsigned long int i)
{
    *buf++ = i>>24; *buf++ = i>>16;
    *buf++ = i>>8;  *buf++ = i;
}

/*
** packi64() -- store a 64-bit int into a char buffer (like htonl())
*/ 
void SOS_buffer_packi64(unsigned char *buf, unsigned long long int i)
{
    *buf++ = i>>56; *buf++ = i>>48;
    *buf++ = i>>40; *buf++ = i>>32;
    *buf++ = i>>24; *buf++ = i>>16;
    *buf++ = i>>8;  *buf++ = i;
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
** pack() -- store data dictated by the format string in the buffer
**
*/

int SOS_buffer_pack(SOS_runtime *sos_context, unsigned char *buf, char *format, ...) {
    SOS_SET_CONTEXT(sos_context, "SOS_buffer_pack");

    va_list ap;

    int      i;           // 32-bit
    long     l;           // 64-bit
    double   d;           // double
    uint64_t fhold;

    char *s;           // strings
    int len;

    int packed_bytes;  // how many bytes have been packed...

    dlog(20, "Packing the following format string: \"%s\"\n", format);

    packed_bytes = 0;

    va_start(ap, format);

    for(; *format != '\0'; format++) {
        switch(*format) {
        case 'i': // 32-bit
            i = va_arg(ap, int);
            dlog(20, "  ... packing i @ %d:   %d   [32-bit]\n", packed_bytes, (int) i);
            SOS_buffer_packi32(buf, i);
            buf += 4;
            packed_bytes += 4;
            break;

        case 'l': // 64-bit
            l = va_arg(ap, long);
            dlog(20, "  ... packing l @ %d:   %ld   [64-bit]\n", packed_bytes, (long) l);
            SOS_buffer_packi64(buf, l);
            buf += 8;
            packed_bytes += 8;
            break;

        case 'd': // float-64
            d = va_arg(ap, double);
            dlog(20, "  ... packing d @ %d:   %lf   [64-bit float]\n", packed_bytes, (double) d);
            fhold = SOS_buffer_pack754_64(d); // convert to IEEE 754
            SOS_buffer_packi64(buf, fhold);
            buf += 8;
            packed_bytes += 8;
            break;

        case 's': // string
            s = va_arg(ap, char*);
            len = strlen(s);
            dlog(20, "  ... packing s @ %d:   \"%s\"   (%d bytes + 4)\n", packed_bytes, s, len);
            SOS_buffer_packi32(buf, len);
            buf += 4;
            packed_bytes += 4;
            memcpy(buf, s, len);
            buf += len;
            packed_bytes += len;
            break;
        }
    }

    va_end(ap);
    dlog(20, "  ... done\n");

    return packed_bytes;
}

/*
** unpack() -- unpack data dictated by the format string into the buffer
**
**  This function returns the number of bytes in the buffer that were read by
**  following the instructions in the format string.
**
*/
int SOS_buffer_unpack(SOS_runtime *sos_context, unsigned char *buf, char *format, ...) {
    SOS_SET_CONTEXT(sos_context, "SOS_buffer_unpack");

    va_list ap;

    int    *i;       // 32-bit
    long   *l;       // 64-bit
    double *d;       // double (64-bit)
    uint64_t fhold;

    char *s;
    unsigned int len, maxstrlen=0, count;

    int packed_bytes;

    dlog(20, "Unpacking the following format string: \"%s\"\n", format);

    packed_bytes = 0;

    va_start(ap, format);

    for(; *format != '\0'; format++) {
        switch(*format) {
        case 'i': // 32-bit
            i = va_arg(ap, int*);
            *i = SOS_buffer_unpacki32(buf);
            dlog(20, "  ... unpacked i @ %d:   %d   [32-bit]\n", packed_bytes, *i);
            buf += 4;
            packed_bytes += 4;
            break;
        case 'l': // 64-bit
            l = va_arg(ap, long*);
            *l = SOS_buffer_unpacki64(buf);
            dlog(20, "  ... unpacked l @ %d:   %ld   [64-bit]\n", packed_bytes, *l);
            buf += 8;
            packed_bytes += 8;
            break;
        case 'd': // float-64
            d = va_arg(ap, double*);
            fhold = SOS_buffer_unpacku64(buf);
            *d = SOS_buffer_unpack754_64(fhold);
            dlog(20, "  ... unpacked d @ %d:   %lf   [64-bit double]\n", packed_bytes, *d);
            buf += 8;
            packed_bytes += 8;
            break;
        case 's': // string
            s = va_arg(ap, char*);
            len = SOS_buffer_unpacki32(buf);
            buf += 4;
            packed_bytes += 4;
            if (maxstrlen > 0 && len > maxstrlen) count = maxstrlen - 1;
            else count = len;
            if (s == NULL) {
                s = (char *) calloc((count + 1), sizeof(char));
            }
            memcpy(s, buf, count);
            s[count] = '\0';
            dlog(20, "  ... unpacked s @ %d:   \"%s\"   (%d bytes + 4)\n", packed_bytes, s, len);
            buf += len;
            packed_bytes += len;
            break;

        default:
            if (isdigit(*format)) { // track max str len
                maxstrlen = maxstrlen * 10 + (*format-'0');
            }
        }

        if (!isdigit(*format)) maxstrlen = 0;
    }

    va_end(ap);
    dlog(20, "  ... done\n");
    

    return packed_bytes;
}





/*
 *   Example of use:
 *

 //#define DEBUG
 #ifdef DEBUG
 #include <limits.h>
 #include <float.h>
 #include <assert.h>
 #endif

 int main(void)
 {
 #ifndef DEBUG
 unsigned char buf[1024];
 unsigned char magic;
 int monkeycount;
 long altitude;
 double absurdityfactor;
 char *s = "Great unmitigated Zot!  You've found the Runestaff!";
 char s2[96];
 unsigned int packetsize, ps2;

 packetsize = pack(buf, "CHhlsd", 'B', 0, 37, -5, s, -3490.5);
 packi16(buf+1, packetsize); // store packet size in packet for kicks

 printf("packet is %u bytes\n", packetsize);

 unpack(buf, "CHhl96sd", &magic, &ps2, &monkeycount, &altitude, s2,
 &absurdityfactor);

 printf("'%c' %hhu %u %ld \"%s\" %f\n", magic, ps2, monkeycount,
 altitude, s2, absurdityfactor);

 #else
 unsigned char buf[1024];

 int x;

 long long k, k2;
 long long test64[14] = { 0, -0, 1, 2, -1, -2, 0x7fffffffffffffffll>>1, 0x7ffffffffffffffell, 0x7fffffffffffffffll, -0x7fffffffffffffffll, -0x8000000000000000ll, 9007199254740991ll, 9007199254740992ll, 9007199254740993ll };

 unsigned long long K, K2;
 unsigned long long testu64[14] = { 0, 0, 1, 2, 0, 0, 0xffffffffffffffffll>>1, 0xfffffffffffffffell, 0xffffffffffffffffll, 0, 0, 9007199254740991ll, 9007199254740992ll, 9007199254740993ll };

 long i, i2;
 long test32[14] = { 0, -0, 1, 2, -1, -2, 0x7fffffffl>>1, 0x7ffffffel, 0x7fffffffl, -0x7fffffffl, -0x80000000l, 0, 0, 0 };

 unsigned long I, I2;
 unsigned long testu32[14] = { 0, 0, 1, 2, 0, 0, 0xffffffffl>>1, 0xfffffffel, 0xffffffffl, 0, 0, 0, 0, 0 };

 int j, j2;
 int test16[14] = { 0, -0, 1, 2, -1, -2, 0x7fff>>1, 0x7ffe, 0x7fff, -0x7fff, -0x8000, 0, 0, 0 };

 printf("char bytes: %zu\n", sizeof(char));
 printf("int bytes: %zu\n", sizeof(int));
 printf("long bytes: %zu\n", sizeof(long));
 printf("long long bytes: %zu\n", sizeof(long long));
 printf("float bytes: %zu\n", sizeof(float));
 printf("double bytes: %zu\n", sizeof(double));
 printf("long double bytes: %zu\n", sizeof(long double));

 for(x = 0; x < 14; x++) {
 k = test64[x];
 pack(buf, "q", k);
 unpack(buf, "q", &k2);

 if (k2 != k) {
 printf("64: %lld != %lld\n", k, k2);
 printf("  before: %016llx\n", k);
 printf("  after:  %016llx\n", k2);
 printf("  buffer: %02hhx %02hhx %02hhx %02hhx "
 " %02hhx %02hhx %02hhx %02hhx\n", 
 buf[0], buf[1], buf[2], buf[3],
 buf[4], buf[5], buf[6], buf[7]);
 } else {
 //printf("64: OK: %lld == %lld\n", k, k2);
 }

 K = testu64[x];
 pack(buf, "Q", K);
 unpack(buf, "Q", &K2);

 if (K2 != K) {
 printf("64: %llu != %llu\n", K, K2);
 } else {
 //printf("64: OK: %llu == %llu\n", K, K2);
 }

 i = test32[x];
 pack(buf, "l", i);
 unpack(buf, "l", &i2);

 if (i2 != i) {
 printf("32(%d): %ld != %ld\n", x,i, i2);
 printf("  before: %08lx\n", i);
 printf("  after:  %08lx\n", i2);
 printf("  buffer: %02hhx %02hhx %02hhx %02hhx "
 " %02hhx %02hhx %02hhx %02hhx\n", 
 buf[0], buf[1], buf[2], buf[3],
 buf[4], buf[5], buf[6], buf[7]);
 } else {
 //printf("32: OK: %ld == %ld\n", i, i2);
 }

 I = testu32[x];
 pack(buf, "L", I);
 unpack(buf, "L", &I2);

 if (I2 != I) {
 printf("32(%d): %lu != %lu\n", x,I, I2);
 } else {
 //printf("32: OK: %lu == %lu\n", I, I2);
 }

 j = test16[x];
 pack(buf, "h", j);
 unpack(buf, "h", &j2);

 if (j2 != j) {
 printf("16: %d != %d\n", j, j2);
 } else {
 //printf("16: OK: %d == %d\n", j, j2);
 }
 }

 if (1) {
 long double testf64[8] = { -3490.6677, 0.0, 1.0, -1.0, DBL_MIN*2, DBL_MAX/2, DBL_MIN, DBL_MAX };
 long double f,f2;

 for (i = 0; i < 8; i++) {
 f = testf64[i];
 pack(buf, "g", f);
 unpack(buf, "g", &f2);

 if (f2 != f) {
 printf("f64: %Lf != %Lf\n", f, f2);
 printf("  before: %016llx\n", *((long long*)&f));
 printf("  after:  %016llx\n", *((long long*)&f2));
 printf("  buffer: %02hhx %02hhx %02hhx %02hhx "
 " %02hhx %02hhx %02hhx %02hhx\n", 
 buf[0], buf[1], buf[2], buf[3],
 buf[4], buf[5], buf[6], buf[7]);
 } else {
 //printf("f64: OK: %f == %f\n", f, f2);
 }
 }
 }
 if (1) {
 double testf32[7] = { 0.0, 1.0, -1.0, 10, -3.6677, 3.1875, -3.1875 };
 double f,f2;

 for (i = 0; i < 7; i++) {
 f = testf32[i];
 pack(buf, "d", f);
 unpack(buf, "d", &f2);

 if (f2 != f) {
 printf("f32: %.10f != %.10f\n", f, f2);
 printf("  before: %016llx\n", *((long long*)&f));
 printf("  after:  %016llx\n", *((long long*)&f2));
 printf("  buffer: %02hhx %02hhx %02hhx %02hhx "
 " %02hhx %02hhx %02hhx %02hhx\n", 
 buf[0], buf[1], buf[2], buf[3],
 buf[4], buf[5], buf[6], buf[7]);
 } else {
 //printf("f32: OK: %f == %f\n", f, f2);
 }
 }
 }
 if (1) {
 float testf16[7] = { 0.0, 1.0, -1.0, 10, -10, 3.1875, -3.1875 };
 float f,f2;

 for (i = 0; i < 7; i++) {
 f = testf16[i];
 pack(buf, "f", f);
 unpack(buf, "f", &f2);

 if (f2 != f) {
 printf("f16: %f != %f\n", f, f2);
 printf("  before: %08x\n", *((int*)&f));
 printf("  after:  %08x\n", *((int*)&f2));
 printf("  buffer: %02hhx %02hhx %02hhx %02hhx "
 " %02hhx %02hhx %02hhx %02hhx\n", 
 buf[0], buf[1], buf[2], buf[3],
 buf[4], buf[5], buf[6], buf[7]);
 } else {
 //printf("f16: OK: %f == %f\n", f, f2);
 }
 }
 }
 #endif

 return 0;
 }

*/
