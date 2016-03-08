#ifndef _STRMALLOC_H_
#define _STRMALLOC_H_

#include <stdio.h>

/*
 * routines for maintaining ref-counted strings on the heap
 *
 * when strmalloc() is invoked, the string is first searched for in an
 * associated hash table; if found, the ref count is incremented, and the
 * pointer to the string is returned; if not found, space for the string is
 * allocated using malloc(), the string is copied into that space, the string
 * is inserted into the appropriate hash bucket, and the pointer to the
 * string is returned
 *
 * when strfree() is invoked, the ref count for the string is decremented; if
 * the ref count has gone to 0, then the string is removed from the hash
 * table and the heap space for the string is free()'d
 */

/*
 * returns NULL if problem allocating space on the heap
 */
char *strmalloc(char *s);

/*
 * decrement reference count for string; if it is 0, remove string from
 * the hashtable
 */
void strfree(char *s);

/*
 * print the elements in the hashtable preceded by their ref counts on `fd'
 */
void strdump(FILE *fd);

#endif /* _STRMALLOC_H_ */
