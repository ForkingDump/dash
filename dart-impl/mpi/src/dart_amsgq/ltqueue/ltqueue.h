#ifndef LTQUEUE_H
#define LTQUEUE_H

#include "../faa.h"
#include "../spsc_queue.h"
#include <mpi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int32_t rank;
  uint32_t tag;
} __attribute__((aligned(8))) ltqueue_tree_node_t;

static const int32_t DUMMY_RANK = ~((uint32_t)0);

typedef struct {
  uint32_t timestamp;
  uint32_t tag;
} __attribute__((aligned(8))) ltqueue_timestamp_t;

static const uint32_t MAX_TIMESTAMP = ~((uint32_t)0);

typedef struct {
  char data[512];
  uint32_t timestamp;
} ltqueue_data_t;

typedef struct {
  MPI_Comm comm;
  int self_rank;
  MPI_Aint dequeuer_rank;

  FaaCounter *counter;

  MPI_Win min_timestamp_win;
  ltqueue_timestamp_t *min_timestamp_ptr;

  MPI_Win tree_win;
  ltqueue_tree_node_t *tree_ptr;
  MPI_Info info;

  spsc_queue_t *spsc;
} ltqueue_t;

ltqueue_t *ltqueue_init(MPI_Aint capacity_per_node, MPI_Aint dequeuer_rank,
                        MPI_Comm comm);
void ltqueue_destroy(ltqueue_t *queue);
bool ltqueue_enqueue(ltqueue_t *queue, const char *data, int data_size);
bool ltqueue_dequeue(ltqueue_t *queue, void *output);

#endif
