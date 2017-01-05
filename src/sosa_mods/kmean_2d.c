
/*
 * kmean_2d.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <math.h>

#include <mpi.h>

#define USAGE "./kmean_2d -n <iter_count> -d <iter_delay_usec> -p <iter_points> -r <iter_radii> -o <offset_max>"

// Options:
//
#define CIRCLES 1
#define SQUARES 2
//
#define MODE SQUARES
//


int    g_iter_count;
int    g_iter_delay;
int    g_iter_point;
double g_iter_radii;
double g_offset_max;

#include "sos.h"
#include "sos_debug.h"


double randf(double upto_maxval) {
    return upto_maxval * rand() / (RAND_MAX - 1.);
}


void gen_xy(double *X, double *Y, double radius) {
    double ang, r;

    if (MODE == CIRCLES) {
        ang = randf(2 * M_PI);
        r = randf(radius);
        *X = r * cos(ang);
        *Y = r * sin(ang);
    } else {
        *X = randf(radius);
        *Y = randf(radius);
    }

    return;
}


int main(int argc, char *argv[]) {

    SOS_runtime *my_sos;

    int i, elem, next_elem;
    SOS_pub *pub;
    double time_now;
    double time_start;

    MPI_Init(&argc, &argv);
    int rank; MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
    int size; MPI_Comm_size(MPI_COMM_WORLD, &size); 
    

    /* Process command-line arguments */
    if ( argc < 5 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    g_iter_count = -1;
    g_iter_delay = -1;
    g_iter_point = -1;
    g_iter_radii = -1.0;
    g_offset_max = -1.0;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-n"  ) == 0) {
            g_iter_count  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-p"  ) == 0) {
            g_iter_point = strtod(argv[next_elem], NULL);
         } else if ( strcmp(argv[elem], "-r"  ) == 0) {
            g_iter_radii = strtold(argv[next_elem], NULL);
         } else if ( strcmp(argv[elem], "-o"  ) == 0) {
            g_offset_max = strtold(argv[next_elem], NULL);
          } else if ( strcmp(argv[elem], "-d"  ) == 0) {
            g_iter_delay = strtod(argv[next_elem], NULL);
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if ((g_iter_count < 1)
        || (g_iter_delay < 0)
        || (g_iter_point < 0)
        || (g_iter_radii < 0.000)
        || (g_offset_max < 0.000)) {
        fprintf(stderr, "%s\n", USAGE); exit(1);
    }

    double  offset_x, offset_y;
    double  x, y;
    char XY_NAME[1024];
    char XY_VAL[1024];
    
    snprintf(XY_NAME, 1024, "2D:(X,Y)");

    my_sos = SOS_init( &argc, &argv, SOS_ROLE_CLIENT, SOS_LAYER_APP);
    SOS_SET_CONTEXT(my_sos, "kmean_2d.main");

    srandom(my_sos->my_guid);

    printf("[Rank %d of %d]: Starting kmean_2d ...\n", rank, size); fflush(stdout);

    usleep(200000);

    if (rank == 0) { dlog(0, "Registering points...\n"); }

    pub = SOS_pub_create(my_sos, "kmeans_2d", SOS_NATURE_KMEAN_2D);
    if (rank == 0) { dlog(0, "  ... pub->guid  = %" SOS_GUID_FMT "\n", pub->guid); }

    int iter;
    int pair;

    SOS_TIME( time_start );
    for (iter = 0; iter < g_iter_count; iter++) {
        usleep(g_iter_delay);
        offset_x = (randf(fabs(g_offset_max - g_iter_radii)));
        offset_y = (randf(fabs(g_offset_max - g_iter_radii)));
        for (pair = 0; pair < g_iter_point; pair++) {
            gen_xy(&x, &y, g_iter_radii);
            x += offset_x;
            y += offset_y;
            snprintf(XY_VAL, 1024, "%f, %f", x, y);
            SOS_pack(pub, XY_NAME, SOS_VAL_TYPE_STRING, XY_VAL);
            if (rank == 0) { printf("STEP %d.%d == %s\n", iter, pair, XY_VAL); fflush(stdout); }
            if ((iter == 0) && (pair == 0)) { SOS_announce(pub); }
       } // for: point pairs
       SOS_publish(pub);
 
       if (rank == 0) {
           printf("%d of %d", iter, g_iter_count);
           printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
           fflush(stdout);
       }
    } // for: iterations

    if (rank == 0) {
        printf("\n");
        fflush(stdout);
    }
    dlog(0, "  ... done.\n");

    SOS_finalize(my_sos);
    MPI_Finalize();
    return (EXIT_SUCCESS);
}
