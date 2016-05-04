#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#include "sos_pipe.h"

#define TITLE "DEMO_PIPE"
#define USAGE " ./demo_pipe <write_count>"

#define READER_BUF_SIZE 2048

#define VERBOSE 1

int running;
uint64_t write_count;
uint64_t read_count;
uint64_t sleep_delay;

double writer_start;
double writer_stop;

double reader_start;
double reader_stop;

#define PUT_TIME(__dbl_now)  { struct timeval t; gettimeofday(&t, NULL); __dbl_now = (double)(t.tv_sec + t.tv_usec/1000000.0); }

void *THREAD_write(void *arg);
void *THREAD_read(void *arg);

int main(int argc, char **argv) {
    if (VERBOSE) printf("\n%s\n%s\n", TITLE, USAGE);

    if (argc != 2) {
        printf("ERROR: Invalid number of command line arguments.   (%d)\n", argc);
        return EXIT_FAILURE;
    }
    write_count = atoll(argv[1]);
    read_count = 0;

    if (VERBOSE) printf("\twrite_count == %ld\n", write_count);

    //pipe_t *p = pipe_new(sizeof(uint64_t), 100000000)
    pipe_t *p = pipe_new(sizeof(uint64_t), 0);
    pipe_producer_t *prod  = pipe_producer_new(p);
    pipe_consumer_t *cons  = pipe_consumer_new(p);
    pipe_free(p);
    if (VERBOSE) printf("\tPipe created.\n");

    pthread_t writer;
    pthread_t reader;
    pthread_create(&writer, NULL, (void *) THREAD_write, (void *) prod);
    pthread_create(&reader, NULL, (void *) THREAD_read, (void *) cons);
    if (VERBOSE) printf("\tPthreads on-line.\n");
    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    printf("WRITER: %2.12lf seconds, %2.12lf per push.\n",
           (writer_stop - writer_start), ((writer_stop - writer_start) / write_count));
    printf("READER: %2.12lf seconds, %2.12lf per pop.\n",
           (reader_stop - reader_start), ((reader_stop - reader_start) / (write_count)));



    return EXIT_SUCCESS;
}

void *THREAD_write(void *arg) {
    pipe_producer_t *prod = (pipe_producer_t *) arg;

    PUT_TIME(writer_start);
    uint64_t buffer[10];
    uint64_t index;
    int i;

    for (index = 0; index < write_count; index++) {
        for (i = 0; i < 10; i++) {
            buffer[i] = index;
        }
        if (VERBOSE) printf("#%ld: <<<<< push { %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld }\n", index,
               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
               buffer[5], buffer[6], buffer[7], buffer[8], buffer[9]);

        pipe_push(prod, buffer, 10);
    }
    PUT_TIME(writer_stop);
    pipe_producer_free(prod);

    pthread_exit(NULL);
}


void *THREAD_read(void *arg) {
    pipe_consumer_t *cons = (pipe_consumer_t *) arg;
    uint64_t buffer[10];
    uint64_t in_count = 0;
    size_t i = 0;

    struct timespec til;
    til.tv_sec = sleep_delay;
    til.tv_nsec = 0;
    nanosleep(&til, NULL);

    int loops = 0;


    PUT_TIME(reader_start);
    do {
        if (VERBOSE) printf("#%d: ----> pop buffer = ", loops++);

        in_count = pipe_pop(cons, buffer, 10);
        read_count += in_count;
        if (VERBOSE) printf("{ %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld }    (read_count == %ld)\n",
               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4],
               buffer[5], buffer[6], buffer[7], buffer[8], buffer[9], read_count);

        for (i = 0; i < 10; i++) {
            buffer[i] = -1;
        }

    } while (in_count > 0);
    PUT_TIME(reader_stop);
    pipe_consumer_free(cons);

    pthread_exit(NULL);
}
