#include "ltqueue/ltqueue.h"
#include <alloca.h>
#include <errno.h>
#include <mpi.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <dash/dart/base/assert.h>
#include <dash/dart/base/atomic.h>
#include <dash/dart/base/env.h>
#include <dash/dart/base/logging.h>
#include <dash/dart/base/mutex.h>
#include <dash/dart/if/dart_active_messages.h>
#include <dash/dart/if/dart_communication.h>
#include <dash/dart/if/dart_globmem.h>
#include <dash/dart/mpi/dart_active_messages_priv.h>
#include <dash/dart/mpi/dart_globmem_priv.h>
#include <dash/dart/mpi/dart_team_private.h>

#define PROCESSING_SIGNAL ((int64_t)INT32_MIN)

/**
 * Name of the environment variable specifying the number microseconds caller
 * sleeps between consecutive reads of the active message in a blocking
 * processing call.
 *
 * Type: integral value with optional us, ms, s qualifier
 */
#define DART_AMSGQ_LTQUEUE_SLEEP_ENVSTR "DART_AMSGQ_LTQUEUE_SLEEP"

struct dart_amsgq_impl_data {
  ltqueue_t **queues;
  MPI_Comm comm;
  dart_mutex_t send_mutex;
  dart_mutex_t processing_mutex;
  int comm_rank;
  int comm_size;
};

#ifdef DART_ENABLE_LOGGING
static uint32_t msgcnt = 0;
#endif // DART_ENABLE_LOGGING

static struct timespec sleeptime = {-1, -1};
static int64_t sleep_us = -1;

static dart_ret_t dart_amsg_ltqueue_openq(size_t msg_size, size_t msg_count,
                                          dart_team_t team,
                                          struct dart_amsgq_impl_data **queue) {
  dart_team_data_t *team_data = dart_adapt_teamlist_get(team);
  if (team_data == NULL) {
    DART_LOG_ERROR("dart_gptr_getaddr ! Unknown team %i", team);
    return DART_ERR_INVAL;
  }

  if (sleeptime.tv_sec == -1) {
    sleep_us = dart__base__env__us(DART_AMSGQ_LTQUEUE_SLEEP_ENVSTR, 0);
    sleeptime.tv_sec = sleep_us / 1000000;
    sleeptime.tv_nsec = sleep_us % 1000000;
  }

  struct dart_amsgq_impl_data *res =
      calloc(1, sizeof(struct dart_amsgq_impl_data));
  MPI_Comm_dup(team_data->comm, &res->comm);

  MPI_Comm_rank(res->comm, &res->comm_rank);
  MPI_Comm_size(res->comm, &res->comm_size);

  res->queues = calloc(res->comm_size, sizeof(ltqueue_t *));
  for (int i = 0; i < res->comm_size; ++i) {
    res->queues[i] = ltqueue_init(msg_count, i, res->comm);
  }
  dart__base__mutex_init(&res->send_mutex);
  dart__base__mutex_init(&res->processing_mutex);

  *queue = res;

  return DART_OK;
}

static dart_ret_t dart_amsg_ltqueue_sendbuf(dart_team_unit_t target,
                                            struct dart_amsgq_impl_data *amsgq,
                                            const void *data,
                                            size_t data_size) {
  DART_LOG_DEBUG("dart_amsg_trysend: u:%i ds:%zu", target.id, data_size);
  if (ltqueue_enqueue(amsgq->queues[target.id], data, data_size)) {

    DART_LOG_TRACE("Sent message of size %zu with payload %zu to unit "
                   "%d starting at offset %ld",
                   msg_size, data_size, target.id, offset);

    return DART_OK;
  } else {
    return DART_ERR_AGAIN;
  }
}

static dart_ret_t
amsg_ltqueue_process_internal(struct dart_amsgq_impl_data *amsgq,
                              bool blocking) {
  int64_t tailpos;
  int comm_rank = amsgq->comm_rank;

  if (!blocking) {
    dart_ret_t ret = dart__base__mutex_trylock(&amsgq->processing_mutex);
    if (ret != DART_OK) {
      return DART_ERR_AGAIN;
    }
  } else {
    dart__base__mutex_lock(&amsgq->processing_mutex);
  }

  do {
    char output[512];
    if (!ltqueue_dequeue(amsgq->queues[comm_rank], output)) {
      break;
    }
    struct dart_amsg_header *header = (struct dart_amsg_header *)(output);
    dart__amsgq__process_buffer(output, sizeof(struct dart_amsg_header) +
                                            header->data_size);
  } while (blocking);
  dart__base__mutex_unlock(&amsgq->processing_mutex);
  return DART_OK;
}

static dart_ret_t
dart_amsg_ltqueue_process(struct dart_amsgq_impl_data *amsgq) {
  return amsg_ltqueue_process_internal(amsgq, false);
}

static dart_ret_t
dart_amsg_ltqueue_process_blocking(struct dart_amsgq_impl_data *amsgq,
                                   dart_team_t team) {
  int flag = 0;
  MPI_Request req;

  // keep processing until all incoming messages have been dealt with
  MPI_Ibarrier(amsgq->comm, &req);
  do {
    amsg_ltqueue_process_internal(amsgq, false);
    MPI_Test(&req, &flag, MPI_STATUSES_IGNORE);
    if (!flag && sleep_us > 0) {
      // sleep for the requested number of microseconds
      nanosleep(&sleeptime, NULL);
    }
  } while (!flag);
  amsg_ltqueue_process_internal(amsgq, true);
  MPI_Barrier(amsgq->comm);
  return DART_OK;
}

static dart_ret_t dart_amsg_ltqueue_closeq(struct dart_amsgq_impl_data *amsgq) {
  MPI_Comm_free(&amsgq->comm);

  dart__base__mutex_destroy(&amsgq->send_mutex);
  dart__base__mutex_destroy(&amsgq->processing_mutex);

  for (int i = 0; i < amsgq->comm_size; ++i) {
    ltqueue_destroy(amsgq->queues[i]);
  }
  free(amsgq->queues);

  free(amsgq);

  return DART_OK;
}

dart_ret_t dart_amsg_ltqueue_init(dart_amsgq_impl_t *impl) {
  impl->openq = dart_amsg_ltqueue_openq;
  impl->closeq = dart_amsg_ltqueue_closeq;
  impl->trysend = dart_amsg_ltqueue_sendbuf;
  impl->process = dart_amsg_ltqueue_process;
  impl->process_blocking = dart_amsg_ltqueue_process_blocking;
  return DART_OK;
}
