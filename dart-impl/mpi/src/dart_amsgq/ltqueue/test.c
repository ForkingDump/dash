#include "ltqueue.h"
#include <mpi.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
  MPI_Init(&argc, &argv);
  ltqueue_t *queue = ltqueue_init(10000, 0, MPI_COMM_WORLD);
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  for (int i = 0; i < 50; ++i) {
    char s[512];
    sprintf(s, "%d", i);
    if (!ltqueue_enqueue(queue, s, strlen(s))) {
      printf("Enqueue failed!\n");
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);
  int size;
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  if (rank == 0) {
    for (int i = 0; i < 50 * size; ++i) {
      char output[512];
      if (ltqueue_dequeue(queue, &output)) {
        printf("dequeue %s\n", output);
      } else {
        printf("dequeue NULL\n");
      }
    }
  }
  MPI_Finalize();
}
