#ifndef COMM_H
#define COMM_H

#include <mpi.h>
#include <stdint.h>

#ifdef PROFILE
#include <caliper/cali.h>
#endif

void write_sync(const void *src, size_t size, int disp,
                unsigned int target_rank, MPI_Win win);
void write_async(const void *src, size_t size, int disp,
                 unsigned int target_rank, MPI_Win win);
void read_sync(void *dst, size_t size, int disp, unsigned int target_rank,
               MPI_Win win);
void read_async(void *dst, size_t size, int disp, unsigned int target_rank,
                MPI_Win win);
void awrite_sync(const void *src, size_t size, int disp,
                 unsigned int target_rank, MPI_Win win);
void awrite_async(const void *src, size_t size, int disp,
                  unsigned int target_rank, MPI_Win win);
void aread_sync(void *dst, size_t size, int disp, unsigned int target_rank,
                MPI_Win win);
void aread_async(void *dst, size_t size, int disp, unsigned int target_rank,
                 MPI_Win win);
void fetch_and_add_sync_int64(int64_t *dst, uint64_t increment, int disp,
                              unsigned int target_rank, MPI_Win win);
void fetch_and_add_sync_uint64(uint64_t *dst, uint64_t increment, int disp,
                               unsigned int target_rank, MPI_Win win);
void compare_and_swap_sync_int64(const int64_t *old_val, const int64_t *new_val,
                                 int64_t *result, int disp,
                                 unsigned int target_rank, MPI_Win win);
void compare_and_swap_sync_uint64(const uint64_t *old_val,
                                  const uint64_t *new_val, uint64_t *result,
                                  int disp, unsigned int target_rank,
                                  MPI_Win win);
void flush(unsigned int rank, MPI_Win win);

#endif /* COMM_H */
