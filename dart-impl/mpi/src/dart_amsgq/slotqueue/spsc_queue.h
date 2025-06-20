#ifndef SPSC_QUEUE_H
#define SPSC_QUEUE_H

#include <mpi.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int self_rank;
  MPI_Aint dequeuer_rank;
  MPI_Aint capacity;

  MPI_Win data_win;
  void *data_ptr;
  size_t data_size; /* size of each data element */

  MPI_Win first_win;
  MPI_Aint *first_ptr;
  MPI_Aint *first_buf;

  MPI_Win last_win;
  MPI_Aint *last_ptr;
  MPI_Win enqueuer_local_last_win;
  MPI_Aint *enqueuer_local_last_ptr;
  MPI_Aint *last_buf;

  MPI_Info info;

  int comm_size;
  MPI_Aint batch_size;
  void **cached_data;
  MPI_Aint *cached_size;
} spsc_queue_t;

spsc_queue_t *spsc_queue_create(MPI_Aint capacity, MPI_Aint dequeuer_rank,
                                MPI_Comm comm, size_t data_size,
                                MPI_Aint batch_size);

void spsc_queue_destroy(spsc_queue_t *queue);

int spsc_queue_enqueue(spsc_queue_t *queue, const void *data);

int spsc_queue_e_read_front(spsc_queue_t *queue, void *output);

int spsc_queue_dequeue(spsc_queue_t *queue, void *output, int enqueuer_rank);

int spsc_queue_d_read_front(spsc_queue_t *queue, void *output,
                            int enqueuer_rank);
#endif /* SPSC_QUEUE_H */
