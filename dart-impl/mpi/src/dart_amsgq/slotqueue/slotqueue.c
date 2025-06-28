#include "./slotqueue.h"
#include "./comm.h"
#include "./spsc_queue.h"
#include "faa.h"
#include <mpi.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const timestamp_t MAX_TIMESTAMP = ~((uint64_t)0);
static const MPI_Aint DUMMY_RANK = ~((MPI_Aint)0);

SlotQueue *slot_queue_init(MPI_Aint capacity_per_node, MPI_Aint dequeuer_rank,
                           MPI_Comm comm) {
  SlotQueue *queue = malloc(sizeof(SlotQueue));
  queue->comm = comm;
  queue->dequeuer_rank = dequeuer_rank;
  queue->min_timestamp_win = MPI_WIN_NULL;
  queue->min_timestamp_ptr = NULL;
  queue->min_timestamp_buf = NULL;
  queue->info = MPI_INFO_NULL;
  queue->counter = NULL;

  int size;
  MPI_Comm_rank(comm, &queue->self_rank);
  MPI_Comm_size(comm, &size);
  queue->size = size;

  queue->spsc = spsc_queue_create(capacity_per_node, dequeuer_rank, comm,
                                  sizeof(data_t), 50);
  queue->counter = faa_counter_create(dequeuer_rank, comm);

  MPI_Info_create(&queue->info);
  MPI_Info_set(queue->info, "same_disp_unit", "true");
  MPI_Info_set(queue->info, "accumulate_ordering", "none");

  if (queue->self_rank == queue->dequeuer_rank) {
    MPI_Win_allocate(queue->size * sizeof(timestamp_t), sizeof(timestamp_t),
                     queue->info, comm, &queue->min_timestamp_ptr,
                     &queue->min_timestamp_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->min_timestamp_win);

    for (int i = 0; i < queue->size; ++i) {
      queue->min_timestamp_ptr[i] = MAX_TIMESTAMP;
    }
    queue->min_timestamp_buf = malloc(queue->size * sizeof(timestamp_t));
  } else {
    MPI_Win_allocate(0, sizeof(timestamp_t), queue->info, comm,
                     &queue->min_timestamp_ptr, &queue->min_timestamp_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->min_timestamp_win);
  }

  MPI_Win_flush_all(queue->min_timestamp_win);
  MPI_Barrier(comm);
  MPI_Win_flush_all(queue->min_timestamp_win);

  return queue;
}

void slot_queue_destroy(SlotQueue *queue) {
  if (!queue)
    return;

  if (queue->min_timestamp_win != MPI_WIN_NULL) {
    MPI_Win_unlock_all(queue->min_timestamp_win);
    MPI_Win_free(&queue->min_timestamp_win);
  }
  if (queue->counter != NULL) {
    faa_counter_destroy(queue->counter);
  }
  if (queue->info != MPI_INFO_NULL) {
    MPI_Info_free(&queue->info);
  }
  if (queue->self_rank == queue->dequeuer_rank) {
    free(queue->min_timestamp_buf);
  }

  spsc_queue_destroy(queue->spsc);
}

static bool slot_queue_refresh_enqueue(SlotQueue *queue, timestamp_t ts) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  data_t front;
  timestamp_t new_timestamp;

  /* Avoid possibly redundant remote read below */
  if (!spsc_queue_e_read_front(queue->spsc, &front)) {
    new_timestamp = MAX_TIMESTAMP;
  } else {
    new_timestamp = front.timestamp;
  }
  if (new_timestamp != ts) {
    return true;
  }

  timestamp_t old_timestamp;
  fetch_and_add_sync_uint64(&old_timestamp, 0, queue->self_rank,
                            queue->dequeuer_rank, queue->min_timestamp_win);
  if (!spsc_queue_e_read_front(queue->spsc, &front)) {
    new_timestamp = MAX_TIMESTAMP;
  } else {
    new_timestamp = front.timestamp;
  }
  if (new_timestamp != ts) {
    return true;
  }

  timestamp_t result;
  compare_and_swap_sync_uint64(&old_timestamp, &new_timestamp, &result,
                               queue->self_rank, queue->dequeuer_rank,
                               queue->min_timestamp_win);
  return result == old_timestamp;
}

static MPI_Aint slot_queue_read_minimum_rank(SlotQueue *queue) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif

  MPI_Aint rank = DUMMY_RANK;
  timestamp_t min_timestamp = MAX_TIMESTAMP;

  for (int i = 0; i < queue->size; ++i) {
    aread_async(&queue->min_timestamp_buf[i], sizeof(timestamp_t), i,
                queue->self_rank, queue->min_timestamp_win);
  }
  flush(queue->self_rank, queue->min_timestamp_win);

  for (int i = 0; i < queue->size; ++i) {
    timestamp_t timestamp = queue->min_timestamp_buf[i];
    if (timestamp < min_timestamp) {
      rank = i;
      min_timestamp = timestamp;
    }
  }
  if (rank == DUMMY_RANK) {
    return DUMMY_RANK;
  }

  for (int i = 0; i < rank; ++i) {
    aread_async(&queue->min_timestamp_buf[i], sizeof(timestamp_t), i,
                queue->self_rank, queue->min_timestamp_win);
  }
  flush(queue->self_rank, queue->min_timestamp_win);

  for (int i = 0; i < rank; ++i) {
    timestamp_t timestamp = queue->min_timestamp_buf[i];
    if (timestamp < min_timestamp) {
      rank = i;
      min_timestamp = timestamp;
    }
  }
  return rank;
}

static bool slot_queue_refresh_dequeue(SlotQueue *queue, MPI_Aint rank) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif

  timestamp_t old_timestamp;
  fetch_and_add_sync_uint64(&old_timestamp, 0, rank, queue->self_rank,
                            queue->min_timestamp_win);

  data_t front;
  timestamp_t new_timestamp;
  if (!spsc_queue_d_read_front(queue->spsc, &front, rank)) {
    new_timestamp = MAX_TIMESTAMP;
  } else {
    new_timestamp = front.timestamp;
  }

  timestamp_t result;
  compare_and_swap_sync_uint64(&old_timestamp, &new_timestamp, &result, rank,
                               queue->self_rank, queue->min_timestamp_win);
  return result == old_timestamp;
}

bool slot_queue_enqueue(SlotQueue *queue, const char *data, int data_size) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif

  if (!queue || !data)
    return false;

  timestamp_t counter = faa_counter_get_and_increment(queue->counter);

  data_t timestamped_data = {};
  memcpy(timestamped_data.data, data, data_size);
  timestamped_data.timestamp = counter;
  bool res = spsc_queue_enqueue(queue->spsc, &timestamped_data);
  if (!res) {
    return false;
  }

  if (!slot_queue_refresh_enqueue(queue, counter)) {
    slot_queue_refresh_enqueue(queue, counter);
  }
  return res;
}

bool slot_queue_dequeue(SlotQueue *queue, void *output) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif

  if (!queue || !output)
    return false;

  MPI_Aint rank = slot_queue_read_minimum_rank(queue);
  if (rank == DUMMY_RANK) {
    return false;
  }

  data_t output_data;
  bool res = spsc_queue_dequeue(queue->spsc, &output_data, rank);
  if (!res) {
    return false;
  }

  memcpy(output, output_data.data, 512);

  if (!slot_queue_refresh_dequeue(queue, rank)) {
    slot_queue_refresh_dequeue(queue, rank);
  }
  return true;
}
