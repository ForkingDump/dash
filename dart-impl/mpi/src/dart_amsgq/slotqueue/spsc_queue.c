#include "./spsc_queue.h"
#include "./comm.h"
#include "mpi.h"

spsc_queue_t *spsc_queue_create(MPI_Aint capacity, MPI_Aint dequeuer_rank,
                                MPI_Comm comm, size_t data_size,
                                MPI_Aint batch_size) {
  spsc_queue_t *queue = malloc(sizeof(spsc_queue_t));
  queue->dequeuer_rank = dequeuer_rank;
  queue->capacity = capacity;
  queue->data_size = data_size;
  queue->batch_size = (batch_size > 0) ? batch_size : 10;
  queue->data_win = MPI_WIN_NULL;
  queue->first_win = MPI_WIN_NULL;
  queue->last_win = MPI_WIN_NULL;
  queue->enqueuer_local_last_win = MPI_WIN_NULL;
  queue->info = MPI_INFO_NULL;

  MPI_Comm_rank(comm, &queue->self_rank);
  MPI_Comm_size(comm, &queue->comm_size);

  queue->first_buf = (MPI_Aint *)calloc(queue->comm_size, sizeof(MPI_Aint));
  queue->last_buf = (MPI_Aint *)calloc(queue->comm_size, sizeof(MPI_Aint));
  if (!queue->first_buf || !queue->last_buf) {
    spsc_queue_destroy(queue);
    return NULL;
  }
  for (int i = 0; i < queue->comm_size; ++i) {
    queue->first_buf[i] = 0;
    queue->last_buf[i] = 0;
  }

  MPI_Info_create(&queue->info);
  MPI_Info_set(queue->info, "same_disp_unit", "true");
  MPI_Info_set(queue->info, "accumulate_ordering", "none");

  if (queue->self_rank == dequeuer_rank) {
    MPI_Win_allocate(capacity * data_size, data_size, queue->info, comm,
                     &queue->data_ptr, &queue->data_win);
    MPI_Win_allocate(queue->comm_size * sizeof(MPI_Aint), sizeof(MPI_Aint),
                     queue->info, comm, &queue->first_ptr, &queue->first_win);
    MPI_Win_allocate(queue->comm_size * sizeof(MPI_Aint), sizeof(MPI_Aint),
                     queue->info, comm, &queue->last_ptr, &queue->last_win);
    MPI_Win_allocate(0, sizeof(MPI_Aint), queue->info, comm,
                     &queue->enqueuer_local_last_ptr,
                     &queue->enqueuer_local_last_win);

    queue->cached_data = (void **)calloc(queue->comm_size, sizeof(void *));
    queue->cached_size = (MPI_Aint *)calloc(queue->comm_size, sizeof(MPI_Aint));
    if (!queue->cached_data || !queue->cached_size) {
      spsc_queue_destroy(queue);
      return NULL;
    }

    for (int i = 0; i < queue->comm_size; ++i) {
      queue->cached_data[i] = malloc(data_size * queue->batch_size);
      if (!queue->cached_data[i]) {
        spsc_queue_destroy(queue);
        return NULL;
      }
    }

    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->first_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->last_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->data_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->enqueuer_local_last_win);

    for (int i = 0; i < queue->comm_size; ++i) {
      queue->first_ptr[i] = 0;
      queue->last_ptr[i] = 0;
    }
  } else {
    MPI_Win_allocate(capacity * data_size, data_size, queue->info, comm,
                     &queue->data_ptr, &queue->data_win);
    MPI_Win_allocate(0, sizeof(MPI_Aint), queue->info, comm, &queue->first_ptr,
                     &queue->first_win);
    MPI_Win_allocate(0, sizeof(MPI_Aint), queue->info, comm, &queue->last_ptr,
                     &queue->last_win);
    MPI_Win_allocate(sizeof(MPI_Aint), sizeof(MPI_Aint), queue->info, comm,
                     &queue->enqueuer_local_last_ptr,
                     &queue->enqueuer_local_last_win);

    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->first_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->last_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->data_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->enqueuer_local_last_win);

    *queue->enqueuer_local_last_ptr = 0;
  }

  MPI_Win_flush_all(queue->data_win);
  MPI_Win_flush_all(queue->first_win);
  MPI_Win_flush_all(queue->last_win);
  MPI_Win_flush_all(queue->enqueuer_local_last_win);
  MPI_Barrier(comm);
  MPI_Win_flush_all(queue->data_win);
  MPI_Win_flush_all(queue->first_win);
  MPI_Win_flush_all(queue->last_win);
  MPI_Win_flush_all(queue->enqueuer_local_last_win);

  return queue;
}

void spsc_queue_destroy(spsc_queue_t *queue) {
  if (!queue)
    return;

  if (queue->data_win != MPI_WIN_NULL) {
    MPI_Win_unlock_all(queue->first_win);
    MPI_Win_unlock_all(queue->last_win);
    MPI_Win_unlock_all(queue->enqueuer_local_last_win);
    MPI_Win_unlock_all(queue->data_win);
    MPI_Win_free(&queue->data_win);
    MPI_Win_free(&queue->first_win);
    MPI_Win_free(&queue->last_win);
    MPI_Win_free(&queue->enqueuer_local_last_win);
  }

  if (queue->info != MPI_INFO_NULL) {
    MPI_Info_free(&queue->info);
  }

  if (queue->self_rank == queue->dequeuer_rank && queue->cached_data) {
    for (int i = 0; i < queue->comm_size; ++i) {
      free(queue->cached_data[i]);
    }
    free(queue->cached_data);
    free(queue->cached_size);
  }

  free(queue->first_buf);
  free(queue->last_buf);

  free(queue);
}

int spsc_queue_enqueue(spsc_queue_t *queue, const void *data) {
  if (!queue || !data)
    return 0;

  MPI_Aint new_last = queue->last_buf[queue->self_rank] + 1;

  if (new_last - queue->first_buf[queue->self_rank] > queue->capacity) {
    aread_sync(&queue->first_buf[queue->self_rank], sizeof(MPI_Aint),
               queue->self_rank, queue->dequeuer_rank, queue->first_win);
    if (new_last - queue->first_buf[queue->self_rank] > queue->capacity) {
      return 0;
    }
  }

  awrite_sync(data, queue->data_size,
              queue->last_buf[queue->self_rank] % queue->capacity,
              queue->self_rank, queue->data_win);
  awrite_sync(&new_last, sizeof(MPI_Aint), 0, queue->self_rank,
              queue->enqueuer_local_last_win);

  if (new_last % 10 == 0) {
    awrite_sync(&new_last, sizeof(MPI_Aint), queue->self_rank,
                queue->dequeuer_rank, queue->last_win);
  }
  queue->last_buf[queue->self_rank] = new_last;

  return 1;
}

int spsc_queue_e_read_front(spsc_queue_t *queue, void *output) {
  if (!queue || !output)
    return 0;

  if (queue->first_buf[queue->self_rank] >= queue->last_buf[queue->self_rank]) {
    return 0;
  }
  aread_sync(&queue->first_buf[queue->self_rank], sizeof(MPI_Aint),
             queue->self_rank, queue->dequeuer_rank, queue->first_win);
  if (queue->first_buf[queue->self_rank] >= queue->last_buf[queue->self_rank]) {
    return 0;
  }

  aread_sync(output, queue->data_size,
             queue->first_buf[queue->self_rank] % queue->capacity,
             queue->self_rank, queue->data_win);

  return 1;
}

int spsc_queue_dequeue(spsc_queue_t *queue, void *output, int enqueuer_rank) {
  if (!queue || !output || enqueuer_rank < 0 ||
      enqueuer_rank >= queue->comm_size)
    return 0;

  MPI_Aint new_first = queue->first_buf[enqueuer_rank] + 1;
  if (new_first > queue->last_buf[enqueuer_rank]) {
    aread_sync(&queue->last_buf[enqueuer_rank], sizeof(MPI_Aint), enqueuer_rank,
               queue->self_rank, queue->last_win);
    if (new_first > queue->last_buf[enqueuer_rank]) {
      aread_sync(&queue->last_buf[enqueuer_rank], sizeof(MPI_Aint), 0,
                 enqueuer_rank, queue->enqueuer_local_last_win);
      if (new_first > queue->last_buf[enqueuer_rank]) {
        return 0;
      }
    }
  }

  if (queue->cached_size[enqueuer_rank] > 0) {
    char *cache_ptr = (char *)queue->cached_data[enqueuer_rank];
    MPI_Aint cache_idx = queue->cached_size[enqueuer_rank] - 1;
    memcpy(output, cache_ptr + cache_idx * queue->data_size, queue->data_size);
    --queue->cached_size[enqueuer_rank];
  } else {
    MPI_Aint offset = (queue->first_buf[enqueuer_rank] % queue->capacity);
    aread_async(output, queue->data_size, offset, enqueuer_rank,
                queue->data_win);

    MPI_Aint nreads =
        (queue->batch_size < queue->last_buf[enqueuer_rank] - new_first)
            ? queue->batch_size
            : queue->last_buf[enqueuer_rank] - new_first;
    queue->cached_size[enqueuer_rank] = nreads;

    char *cache_ptr = (char *)queue->cached_data[enqueuer_rank];
    for (MPI_Aint i = 0; i < nreads; ++i) {
      MPI_Aint cache_idx = nreads - i - 1;
      MPI_Aint data_offset = ((new_first + i) % queue->capacity);
      aread_async(cache_ptr + cache_idx * queue->data_size, queue->data_size,
                  data_offset, enqueuer_rank, queue->data_win);
    }
    flush(enqueuer_rank, queue->data_win);
  }

  awrite_sync(&new_first, sizeof(MPI_Aint), enqueuer_rank, queue->self_rank,
              queue->first_win);
  queue->first_buf[enqueuer_rank] = new_first;

  return 1;
}

int spsc_queue_d_read_front(spsc_queue_t *queue, void *output,
                            int enqueuer_rank) {
  if (!queue || !output || enqueuer_rank < 0 ||
      enqueuer_rank >= queue->comm_size)
    return 0;

  if (queue->first_buf[enqueuer_rank] >= queue->last_buf[enqueuer_rank]) {
    aread_sync(&queue->last_buf[enqueuer_rank], sizeof(MPI_Aint), enqueuer_rank,
               queue->self_rank, queue->last_win);
    if (queue->first_buf[enqueuer_rank] >= queue->last_buf[enqueuer_rank]) {
      aread_sync(&queue->last_buf[enqueuer_rank], sizeof(MPI_Aint), 0,
                 enqueuer_rank, queue->enqueuer_local_last_win);
      if (queue->first_buf[enqueuer_rank] >= queue->last_buf[enqueuer_rank]) {
        return 0;
      }
    }
  }

  if (queue->cached_size[enqueuer_rank] <= 0) {
    MPI_Aint nreads =
        (queue->batch_size <
         queue->last_buf[enqueuer_rank] - queue->first_buf[enqueuer_rank])
            ? queue->batch_size
            : queue->last_buf[enqueuer_rank] - queue->first_buf[enqueuer_rank];
    queue->cached_size[enqueuer_rank] = nreads;

    char *cache_ptr = (char *)queue->cached_data[enqueuer_rank];
    for (MPI_Aint i = 0; i < nreads; ++i) {
      MPI_Aint cache_idx = nreads - i - 1;
      MPI_Aint data_offset =
          ((queue->first_buf[enqueuer_rank] + i) % queue->capacity);
      aread_async(cache_ptr + cache_idx * queue->data_size, queue->data_size,
                  data_offset, enqueuer_rank, queue->data_win);
    }
    flush(enqueuer_rank, queue->data_win);
  }

  char *cache_ptr = (char *)queue->cached_data[enqueuer_rank];
  MPI_Aint cache_idx = queue->cached_size[enqueuer_rank] - 1;
  memcpy(output, cache_ptr + cache_idx * queue->data_size, queue->data_size);

  return 1;
}
