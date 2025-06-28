#ifndef SQ_QUEUE_H
#define SQ_QUEUE_H

#include "./comm.h"
#include "./spsc_queue.h"
#include "faa.h"
#include <mpi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef uint64_t timestamp_t;

typedef struct {
  char data[512];
  uint64_t timestamp;
} data_t;

typedef struct {
  MPI_Comm comm;
  MPI_Aint size;
  int self_rank;
  MPI_Aint dequeuer_rank;

  FaaCounter *counter;

  MPI_Win min_timestamp_win;
  timestamp_t *min_timestamp_ptr;
  timestamp_t *min_timestamp_buf;

  MPI_Info info;

  spsc_queue_t *spsc;
} SlotQueue;

SlotQueue *slot_queue_init(MPI_Aint capacity_per_node, MPI_Aint dequeuer_rank,
                           MPI_Comm comm);

void slot_queue_destroy(SlotQueue *queue);

bool slot_queue_enqueue(SlotQueue *queue, const char *data, int data_size);

bool slot_queue_dequeue(SlotQueue *queue, void *output);

#endif /* SQ_QUEUE_H */
