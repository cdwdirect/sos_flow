/*
 * reads one or more files, or standard input if no arguments
 *
 * breaks each file into words;
 * each word is stored away using strmalloc()
 * after all files are processed, calls strdump() to print out the ref counts
 * all words are then freed using strfree()
 * finally, strdump() is called to show that all strings have been returned
 */

#include "strmalloc.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define IN 1	/* inside a word */
#define OUT 0	/* outside a word */

typedef struct link {
    struct link *next;
    char *st;
} Link;

static void put_word(char *word, Link **ll) {
    Link *p;

    (void)strmalloc(word);	/* store it away */
    p = (Link *)malloc(sizeof(Link));	/* assume success of malloc & strdup */
    p->st = strdup(word);
    p->next = *ll;
    *ll = p;
}

/*
 * break up file into words (sequences of alphas)
 * each word is stored using strmalloc() - it is also placed in 'll' for
 * eventual freeing
 */

static void process_file(FILE *fd, Link **ll) {
    int c, state;
    char word[1024], *p;

    state = OUT;
    while ((c = fgetc(fd)) != EOF) {
        if (isalpha(c)) {
            if (state == OUT) {
                state = IN;
                p = word;
            }
            *p++ = c;
        } else if (state == IN) {	/* ! alpha and were IN */
            *p = '\0';
            put_word(word, ll);
            state = OUT;
        }
    }
    if (state == IN) {
        *p = '\0';
        put_word(word, ll);
    }
}

int main(int argc, char *argv[]) {
    Link *ll = NULL;
    Link *p;

    if (argc == 1)
        process_file(stdin, &ll);
    else {
        int i;
        FILE *fd;

        for (i = 1; i < argc; i++) {
            fd = fopen(argv[i], "r");
            if (fd == NULL) {
                fprintf(stderr, "Unable to open %s\n", argv[i]);
                continue;
            }
            process_file(fd, &ll);
            fclose(fd);
        }
    }
    printf("Contents of string hash table after strmalloc() calls\n");
    strdump(stdout);
    while (ll != NULL) {
        p = ll;
        ll = p->next;
        strfree(p->st);
        free(p->st);
        free(p);
    }
    printf("Contents of string hash table after strfree() calls\n");
    strdump(stdout);
    return 0;
}
