#include "strmalloc.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define TABLE_SIZE 32767

/* nodes used to link strings into buckets */
typedef struct node {
   struct node *next;
   int refcount;
   char *str;
} Node;

/*
 * 'lock' is used to create critical sections around access/mods to hashTable
 */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static Node *hashTable[TABLE_SIZE] = {NULL};

/*
 * Paul Larson's hash function (same as Kernighan's with 101 replacing 31)
 */
static unsigned int hash(char *s) {
   unsigned int ans = 0;

   while (*s != '\0')
      ans = 101 * ans + *s++;
   return ans % TABLE_SIZE;
}

char *strmalloc(char *s) {
   unsigned int hval = hash(s);
   Node *p;
   char *ans;

   pthread_mutex_lock(&lock);
   for (p = hashTable[hval]; p != NULL; p=p->next)
      if (strcmp(s, p->str) == 0) {
         ans = p->str;
         p->refcount++;
	 pthread_mutex_unlock(&lock);
	 return ans;
      }
   /* string not found, must malloc, copy, and insert into bucket */
   p = (Node *)malloc(sizeof(Node));
   if (! p) {
      pthread_mutex_unlock(&lock);
      return NULL;
   }
   p->str = strdup(s);
   if (! p->str) {
      free(p);
      pthread_mutex_unlock(&lock);
      return NULL;
   }
   ans = p->str;
   p->refcount = 1;
   p->next = hashTable[hval];
   hashTable[hval] = p;
   pthread_mutex_unlock(&lock);
   return ans;
}

void strfree(char *s) {
   unsigned int hval = hash(s);
   Node *p, *q;

   pthread_mutex_lock(&lock);
   for (q = NULL, p = hashTable[hval]; p != NULL; q = p, p=q->next)
      if (strcmp(s, p->str) == 0) {
         if (--p->refcount <= 0) {
            if (q == NULL)
               hashTable[hval] = p->next;
	    else
               q->next = p->next;
	    free(p->str);
	    free(p);
	    break;
	 }
      }
   pthread_mutex_unlock(&lock);
}

void strdump(FILE *fd) {
   int i;

   pthread_mutex_lock(&lock);
   for (i = 0; i < TABLE_SIZE; i++) {
      Node *p;
      for (p = hashTable[i]; p != NULL; p = p->next)
         fprintf(fd, "%8d %s\n", p->refcount, p->str);
   }
   pthread_mutex_unlock(&lock);
}
