#include "./comm.h"
#include <mpi.h>

void write_sync(const void *src, size_t size, int disp,
                unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Put(src, size, MPI_CHAR, target_rank, disp, size, MPI_CHAR, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void write_async(const void *src, size_t size, int disp,
                 unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Put(src, size, MPI_CHAR, target_rank, disp, size, MPI_CHAR, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void read_sync(void *dst, size_t size, int disp, unsigned int target_rank,
               MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Get(dst, size, MPI_CHAR, target_rank, disp, size, MPI_CHAR, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void read_async(void *dst, size_t size, int disp, unsigned int target_rank,
                MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Get(dst, size, MPI_CHAR, target_rank, disp, size, MPI_CHAR, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void awrite_sync(const void *src, size_t size, int disp,
                 unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Accumulate(src, size, MPI_CHAR, target_rank, disp, size, MPI_CHAR,
                 MPI_REPLACE, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void awrite_async(const void *src, size_t size, int disp,
                  unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Accumulate(src, size, MPI_CHAR, target_rank, disp, size, MPI_CHAR,
                 MPI_REPLACE, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void aread_sync(void *dst, size_t size, int disp, unsigned int target_rank,
                MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Get_accumulate(NULL, 0, MPI_INT, dst, size, MPI_CHAR, target_rank, disp,
                     size, MPI_CHAR, MPI_NO_OP, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void aread_async(void *dst, size_t size, int disp, unsigned int target_rank,
                 MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Get_accumulate(NULL, 0, MPI_INT, dst, size, MPI_CHAR, target_rank, disp,
                     size, MPI_CHAR, MPI_NO_OP, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void fetch_and_add_sync_int64(void *dst, uint64_t increment, int disp,
                              unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  uint64_t inc = increment;
  MPI_Fetch_and_op(&inc, dst, MPI_INT64_T, target_rank, disp, MPI_SUM, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void fetch_and_add_sync_uint64(void *dst, uint64_t increment, int disp,
                               unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  uint64_t inc = increment;
  MPI_Fetch_and_op(&inc, dst, MPI_UINT64_T, target_rank, disp, MPI_SUM, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void compare_and_swap_sync_int64(const void *old_val, const void *new_val,
                                 void *result, int disp,
                                 unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Compare_and_swap(new_val, old_val, result, MPI_INT64_T, target_rank, disp,
                       win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void compare_and_swap_sync_uint64(const void *old_val, const void *new_val,
                                  void *result, int disp,
                                  unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Compare_and_swap(new_val, old_val, result, MPI_UINT64_T, target_rank,
                       disp, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void compare_and_swap_sync_uint32(const void *old_val, const void *new_val,
                                  void *result, int disp,
                                  unsigned int target_rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Compare_and_swap(new_val, old_val, result, MPI_UINT32_T, target_rank,
                       disp, win);
  MPI_Win_flush(target_rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}

void flush(unsigned int rank, MPI_Win win) {
#ifdef PROFILE
  CALI_MARK_FUNCTION_BEGIN;
#endif
  MPI_Win_flush(rank, win);
#ifdef PROFILE
  CALI_MARK_FUNCTION_END;
#endif
}
