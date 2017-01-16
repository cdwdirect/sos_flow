#ifndef SOS_BUFFER_H
#define SOS_BUFFER_H


/*
 *   Serialization code for numeric data.
 *     ... drawn from Beej's guide.
 *     ... modified to prepend SOS_buffer_* to the function signatures.
 *                (2015, Chad Wood)
 *
 *   See: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 */


#include <stdio.h>
#include <pthread.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "sos.h"
#include "sos_types.h"

typedef struct {
    void                *sos_context;
    bool                 is_locking;
    pthread_mutex_t     *lock;
    unsigned char       *data;
    int                  len;
    int                  max;
} SOS_buffer;


/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

void         SOS_buffer_init(void *sos_context, SOS_buffer **buffer);
void         SOS_buffer_init_sized(void *sos_context, SOS_buffer **buffer, int max_size);
void         SOS_buffer_init_sized_locking(void *sos_context, SOS_buffer **buffer, int max_size, bool locking);
void         SOS_buffer_clone(SOS_buffer **dest, SOS_buffer *src);
void         SOS_buffer_lock(SOS_buffer *buffer);
void         SOS_buffer_unlock(SOS_buffer *buffer);
void         SOS_buffer_destroy(SOS_buffer *buffer);
             // The following functions do *NOT* lock the buffer...
             // (You should hold the lock already, manually)
void         SOS_buffer_wipe(SOS_buffer *buffer);
void         SOS_buffer_grow(SOS_buffer *buffer, size_t grow_amount, char *from_func);
void         SOS_buffer_trim(SOS_buffer *buffer, size_t to_new_max);
int          SOS_buffer_pack(SOS_buffer *buffer, int *offset, char *format, ...);
int          SOS_buffer_unpack(SOS_buffer *buffer, int *offset, char *format, ...);
void         SOS_buffer_unpack_safestr(SOS_buffer *buffer, int *offset, char **dest);

uint64_t     SOS_buffer_pack754(long double f, unsigned bits, unsigned expbits);
double       SOS_buffer_unpack754(uint64_t i, unsigned bits, unsigned expbits);
void         SOS_buffer_packi32(unsigned char *buf, int32_t i);
void         SOS_buffer_packi64(unsigned char *buf, int64_t i);
int32_t      SOS_buffer_unpacki32(unsigned char *buf);
int64_t      SOS_buffer_unpacki64(unsigned char *buf);
uint64_t     SOS_buffer_unpacku64(unsigned char *buf);

#ifdef __cplusplus
}
#endif

#endif
