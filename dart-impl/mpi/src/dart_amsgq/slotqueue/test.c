#include "slotqueue.h"
#include <mpi.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  SlotQueue *queue = slot_queue_init(10000, 0, MPI_COMM_WORLD);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  for (int i = 0; i < 50; ++i) {
    char s[512];
    sprintf(s, "%d", i);
    if (!slot_queue_enqueue(queue, s, strlen(s))) {
      printf("Enqueue failed!\n");
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (rank == 0) {
    for (int i = 0; i < 50 * size; ++i) {
      char output[512];
      if (slot_queue_dequeue(queue, &output)) {
        printf("dequeue %s\n", output);
      } else {
        printf("dequeue NULL\n");
      }
    }
  }
  MPI_Finalize();
}
