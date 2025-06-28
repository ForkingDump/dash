#pragma once
#include "./faa.h"
#include "./comm.h"
#include <mpi.h>
#include <stdlib.h>

FaaCounter *faa_counter_create(MPI_Aint dequeuer_rank, MPI_Comm comm) {
  FaaCounter *counter = (FaaCounter *)malloc(sizeof(FaaCounter));
  if (!counter)
    return NULL;

  counter->counter_ptr = NULL;
  counter->counter_win = MPI_WIN_NULL;
  counter->info = MPI_INFO_NULL;
  counter->host = 0;

  MPI_Info_create(&counter->info);
  MPI_Info_set(counter->info, "same_disp_unit", "true");
  MPI_Info_set(counter->info, "accumulate_ordering", "none");

  int size;
  MPI_Comm_size(comm, &size);
  counter->host = (dequeuer_rank + 1) % size;

  int rank;
  MPI_Comm_rank(comm, &rank);

  if (counter->host == rank) {
    MPI_Win_allocate(sizeof(MPI_Aint), sizeof(MPI_Aint), counter->info, comm,
                     &counter->counter_ptr, &counter->counter_win);
  } else {
    MPI_Win_allocate(0, sizeof(MPI_Aint), counter->info, comm,
                     &counter->counter_ptr, &counter->counter_win);
  }

  MPI_Win_lock_all(MPI_MODE_NOCHECK, counter->counter_win);

  if (counter->host == rank) {
    *counter->counter_ptr = 0;
  }

  MPI_Win_flush_all(counter->counter_win);
  MPI_Barrier(comm);
  MPI_Win_flush_all(counter->counter_win);

  return counter;
}

void faa_counter_destroy(FaaCounter *counter) {
  if (!counter)
    return;

  if (counter->counter_win != MPI_WIN_NULL) {
    MPI_Win_unlock_all(counter->counter_win);
    MPI_Win_free(&counter->counter_win);
  }

  if (counter->info != MPI_INFO_NULL) {
    MPI_Info_free(&counter->info);
  }

  free(counter);
}

MPI_Aint faa_counter_get_and_increment(FaaCounter *counter) {
  MPI_Aint old_counter;
  fetch_and_add_sync_int64(&old_counter, 1, 0, counter->host,
                           counter->counter_win);
  return old_counter;
}
