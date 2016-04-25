
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
#define USAGE " ./demo_pipe <write_count> <sleep_reader_secs>"

#define READER_BUF_SIZE 2048

int running;
uint64_t write_count;
uint64_t sleep_delay;

double writer_start;
double writer_stop;

double reader_start;
double reader_stop;

#define PUT_TIME(__dbl_now)  { struct timeval t; gettimeofday(&t, NULL); __dbl_now = (double)(t.tv_sec + t.tv_usec/1000000.0); }

void *THREAD_write(void *arg);
void *THREAD_read(void *arg);

int main(int argc, char **argv) {
    printf("\n%s\n%s\n", TITLE, USAGE);

    if (argc != 3) {
        printf("ERROR: Invalid number of command line arguments.   (%d)\n", argc);
        return EXIT_FAILURE;
    }
    write_count = atoll(argv[1]);
    sleep_delay = atoll(argv[2]);

    pipe_t *p = pipe_new(sizeof(uint64_t), 0);
    pipe_producer_t *prod  = { pipe_producer_new(p) };
    pipe_consumer_t *cons  = { pipe_consumer_new(p) };
    pipe_free(p);
    printf("\tPipe created.\n");

    pthread_t writer;
    pthread_t reader;
    pthread_create(&writer, NULL, (void *) THREAD_write, (void *) prod);
    pthread_create(&reader, NULL, (void *) THREAD_read, (void *) cons);
    printf("\tPthreads on-line.\n");
    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    printf("WRITER: %2.12lf seconds, %2.12lf per element.\n",
           (writer_stop - writer_start), ((writer_stop - writer_start) / write_count));
    printf("READER: %2.12lf seconds, %2.12lf per element.\n",
           (reader_stop - reader_start), ((reader_stop - reader_start) / (write_count)));

    return EXIT_SUCCESS;
}

void *THREAD_write(void *arg) {
    pipe_producer_t *prod = (pipe_producer_t *) arg;

    PUT_TIME(writer_start);
    uint64_t thing = 0;
    for (thing = 0; thing < write_count; thing++) {
        pipe_push(prod, (void *) &thing, sizeof(uint64_t));
    }
    PUT_TIME(writer_stop);
    pipe_producer_free(prod);

    pthread_exit(NULL);
}


void *THREAD_read(void *arg) {
    pipe_consumer_t *cons = (pipe_consumer_t *) arg;
    uint64_t buf[READER_BUF_SIZE];
    size_t read = 0;
    size_t i = 0;

    struct timespec til;
    til.tv_sec = sleep_delay;
    til.tv_nsec = 0;
    nanosleep(&til, NULL);

    PUT_TIME(reader_start);
    while ((read = pipe_pop_eager(cons, &buf, READER_BUF_SIZE * sizeof(uint64_t)))) {

    }
    PUT_TIME(reader_stop);
    pipe_consumer_free(cons);

    pthread_exit(NULL);
}
