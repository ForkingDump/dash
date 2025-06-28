#ifndef FAA_H
#define FAA_H

#include <mpi.h>
typedef struct {
  MPI_Aint *counter_ptr;
  MPI_Win counter_win;
  MPI_Info info;
  MPI_Aint host;
} FaaCounter;

FaaCounter *faa_counter_create(MPI_Aint dequeuer_rank, MPI_Comm comm);

void faa_counter_destroy(FaaCounter *counter);

MPI_Aint faa_counter_get_and_increment(FaaCounter *counter);

#endif // FAA_H
