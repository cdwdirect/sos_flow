#include <stdio.h>
#include <string.h>

#include "sos.h"
#define SOS_DEBUG 1

int main(int argc, char **argv) {
    SOS_init(&argc, &argv, SOS_ROLE_CLIENT);

    SOS_ring_queue *ring;

    SOS_ring_init(&ring);         printf("ring->elem_count == %d   init()\n", ring->elem_count);

    SOS_ring_put( ring, 1 );  printf("ring->elem_count == %d   put(1)\n", ring->elem_count);
    SOS_ring_put( ring, 2 );  printf("ring->elem_count == %d   put(2)\n", ring->elem_count);
    SOS_ring_put( ring, 3 );  printf("ring->elem_count == %d   put(3)\n", ring->elem_count);
    SOS_ring_put( ring, 4 );  printf("ring->elem_count == %d   put(4)\n", ring->elem_count);
    SOS_ring_put( ring, 5 );  printf("ring->elem_count == %d   put(5)\n", ring->elem_count);

    printf("SOS_ring_get(ring) == %ld   (ring->elem_count == %d, R@%d, W@%d)\n", SOS_ring_get(ring), ring->elem_count, ring->read_elem, ring->write_elem);
    printf("SOS_ring_get(ring) == %ld   (ring->elem_count == %d, R@%d, W@%d)\n", SOS_ring_get(ring), ring->elem_count, ring->read_elem, ring->write_elem);
    printf("SOS_ring_get(ring) == %ld   (ring->elem_count == %d, R@%d, W@%d)\n", SOS_ring_get(ring), ring->elem_count, ring->read_elem, ring->write_elem);
    printf("SOS_ring_get(ring) == %ld   (ring->elem_count == %d, R@%d, W@%d)\n", SOS_ring_get(ring), ring->elem_count, ring->read_elem, ring->write_elem);
    printf("SOS_ring_get(ring) == %ld   (ring->elem_count == %d, R@%d, W@%d)\n", SOS_ring_get(ring), ring->elem_count, ring->read_elem, ring->write_elem);

    printf("ring->elem_count == %d\n", ring->elem_count);

    SOS_ring_put( ring, 111 );  printf("SOS_ring_put(111)   (ring->elem_count == %d, R@%d, W@%d)\n", ring->elem_count, ring->read_elem, ring->write_elem);
    SOS_ring_put( ring, 222 );  printf("SOS_ring_put(222)   (ring->elem_count == %d, R@%d, W@%d)\n", ring->elem_count, ring->read_elem, ring->write_elem);
    SOS_ring_put( ring, 333 );  printf("SOS_ring_put(333)   (ring->elem_count == %d, R@%d, W@%d)\n", ring->elem_count, ring->read_elem, ring->write_elem);
    SOS_ring_put( ring, 444 );  printf("SOS_ring_put(444)   (ring->elem_count == %d, R@%d, W@%d)\n", ring->elem_count, ring->read_elem, ring->write_elem);
    SOS_ring_put( ring, 555 );  printf("SOS_ring_put(555)   (ring->elem_count == %d, R@%d, W@%d)\n", ring->elem_count, ring->read_elem, ring->write_elem);

    long  *elem_list;
    int    elem_count;
    int    list_index;

    elem_list = SOS_ring_get_all( ring, &elem_count );

    printf("SOS_ring_get_all( ring, &elem_count ) ==\n");
    for (list_index = 0; list_index < elem_count; list_index++) {
        printf("   elem_list[%d] == %ld\n", list_index, elem_list[list_index]);
    }

    printf("   -----\n");
    printf("   ring->elem_count == %d, R@%d, W@%d\n", ring->elem_count, ring->read_elem, ring->write_elem);
   

    SOS_ring_destroy(ring);
    SOS_finalize();

    return(0);
}
