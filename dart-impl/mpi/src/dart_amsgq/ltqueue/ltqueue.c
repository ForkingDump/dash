#include "ltqueue.h"
#include "../comm.h"
#include "../faa.h"
#include <stdlib.h>

static int _get_number_of_processes(const ltqueue_t *queue);
static int _get_tree_size(const ltqueue_t *queue);
static int _get_parent_index(int index);
static int _get_self_index(const ltqueue_t *queue);
static int _get_enqueuer_index(const ltqueue_t *queue, int rank);
static int *_get_children_indexes(const ltqueue_t *queue, int index,
                                  int *count);

static void _e_propagate(ltqueue_t *queue);
static bool _e_refresh_self_node(ltqueue_t *queue);
static bool _e_refresh_timestamp(ltqueue_t *queue);
static bool _e_refresh(ltqueue_t *queue, int current_index);

static bool _d_refresh_timestamp(ltqueue_t *queue, int enqueuer_rank);
static bool _d_refresh_self_node(ltqueue_t *queue, int enqueuer_rank);
static bool _d_refresh(ltqueue_t *queue, int current_index);
static void _d_propagate(ltqueue_t *queue, int enqueuer_rank);

static int _get_number_of_processes(const ltqueue_t *queue) {
  int number_processes;
  MPI_Comm_size(queue->comm, &number_processes);
  return number_processes;
}

static int _get_tree_size(const ltqueue_t *queue) {
  return 2 * _get_number_of_processes(queue);
}

static int _get_parent_index(int index) {
  if (index == 0) {
    return -1;
  }
  return (index - 1) / 2;
}

static int _get_self_index(const ltqueue_t *queue) {
  return _get_number_of_processes(queue) + queue->self_rank;
}

static int _get_enqueuer_index(const ltqueue_t *queue, int rank) {
  return _get_number_of_processes(queue) + rank;
}

static int *_get_children_indexes(const ltqueue_t *queue, int index,
                                  int *count) {
  int left_child = index * 2 + 1;
  int right_child = index * 2 + 2;
  static int children[2];

  *count = 0;

  if (left_child >= _get_tree_size(queue)) {
    return NULL;
  }
  children[(*count)++] = left_child;

  if (right_child < _get_tree_size(queue)) {
    children[(*count)++] = right_child;
  }

  return children;
}

static void _e_propagate(ltqueue_t *queue) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  if (!_e_refresh_self_node(queue)) {
    _e_refresh_self_node(queue);
  }
  int current_index = _get_self_index(queue);
  do {
    current_index = _get_parent_index(current_index);
    if (!_e_refresh(queue, current_index)) {
      _e_refresh(queue, current_index);
    }
  } while (current_index != 0);
}

static bool _e_refresh_self_node(ltqueue_t *queue) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  bool res;
  int self_index = _get_self_index(queue);
  ltqueue_tree_node_t self_node;
  ltqueue_timestamp_t min_timestamp;

  aread_sync(&min_timestamp, sizeof(min_timestamp), queue->self_rank,
             queue->dequeuer_rank, queue->min_timestamp_win);
  aread_sync(&self_node, sizeof(self_node), self_index, queue->dequeuer_rank,
             queue->tree_win);

  if (min_timestamp.timestamp == MAX_TIMESTAMP) {
    const ltqueue_tree_node_t new_node = {DUMMY_RANK, self_node.tag + 1};
    ltqueue_tree_node_t result_node;
    compare_and_swap_sync_uint64(&self_node, &new_node, &result_node,
                                 self_index, queue->dequeuer_rank,
                                 queue->tree_win);
    res =
        result_node.rank == self_node.rank && result_node.tag == self_node.tag;
  } else {
    const ltqueue_tree_node_t new_node = {(int32_t)queue->self_rank, self_node.tag + 1};
    ltqueue_tree_node_t result_node;
    compare_and_swap_sync_uint64(&self_node, &new_node, &result_node,
                                 self_index, queue->dequeuer_rank,
                                 queue->tree_win);
    res =
        result_node.rank == self_node.rank && result_node.tag == self_node.tag;
  }
  return res;
}

static bool _e_refresh_timestamp(ltqueue_t *queue) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  bool res;
  ltqueue_data_t front;
  bool min_timestamp_succeeded = spsc_queue_e_read_front(queue->spsc, &front);

  ltqueue_timestamp_t current_timestamp;
  aread_sync(&current_timestamp, sizeof(current_timestamp), queue->self_rank,
             queue->dequeuer_rank, queue->min_timestamp_win);

  if (!min_timestamp_succeeded) {
    const ltqueue_timestamp_t new_timestamp = {MAX_TIMESTAMP,
                                       current_timestamp.tag + 1};
    ltqueue_timestamp_t result_timestamp;
    compare_and_swap_sync_uint64(
        &current_timestamp, &new_timestamp, &result_timestamp, queue->self_rank,
        queue->dequeuer_rank, queue->min_timestamp_win);
    res = result_timestamp.tag == current_timestamp.tag &&
          result_timestamp.timestamp == current_timestamp.timestamp;
  } else {
    const ltqueue_timestamp_t new_timestamp = {front.timestamp,
                                       current_timestamp.tag + 1};
    ltqueue_timestamp_t result_timestamp;
    compare_and_swap_sync_uint64(
        &current_timestamp, &new_timestamp, &result_timestamp, queue->self_rank,
        queue->dequeuer_rank, queue->min_timestamp_win);
    res = result_timestamp.tag == current_timestamp.tag &&
          result_timestamp.timestamp == current_timestamp.timestamp;
  }
  return res;
}

static bool _e_refresh(ltqueue_t *queue, int current_index) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  ltqueue_tree_node_t current_node;
  uint32_t min_timestamp = MAX_TIMESTAMP;
  int32_t min_timestamp_rank = DUMMY_RANK;

  aread_sync(&current_node, sizeof(current_node), current_index,
             queue->dequeuer_rank, queue->tree_win);

  int child_count;
  int *children = _get_children_indexes(queue, current_index, &child_count);

  for (int i = 0; i < child_count; i++) {
    int child_index = children[i];
    ltqueue_tree_node_t child_node;
    aread_sync(&child_node, sizeof(child_node), child_index,
               queue->dequeuer_rank, queue->tree_win);

    if (child_node.rank == DUMMY_RANK) {
      continue;
    }

    ltqueue_timestamp_t child_timestamp;
    aread_sync(&child_timestamp, sizeof(child_timestamp), child_node.rank,
               queue->dequeuer_rank, queue->min_timestamp_win);

    if (child_timestamp.timestamp < min_timestamp) {
      min_timestamp = child_timestamp.timestamp;
      min_timestamp_rank = child_node.rank;
    }
  }

  const ltqueue_tree_node_t new_node = {min_timestamp_rank, current_node.tag + 1};
  ltqueue_tree_node_t result_node;
  compare_and_swap_sync_uint64(&current_node, &new_node, &result_node,
                               current_index, queue->dequeuer_rank,
                               queue->tree_win);
  return result_node.tag == current_node.tag &&
         result_node.rank == current_node.rank;
}

static bool _d_refresh_timestamp(ltqueue_t *queue, int enqueuer_rank) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  bool res;
  ltqueue_data_t front;
  bool min_timestamp_succeeded =
      spsc_queue_d_read_front(queue->spsc, &front, enqueuer_rank);

  ltqueue_timestamp_t current_timestamp;
  aread_sync(&current_timestamp, sizeof(current_timestamp), enqueuer_rank,
             queue->self_rank, queue->min_timestamp_win);

  if (!min_timestamp_succeeded) {
    const ltqueue_timestamp_t new_timestamp = {MAX_TIMESTAMP,
                                       current_timestamp.tag + 1};
    ltqueue_timestamp_t result_timestamp;
    compare_and_swap_sync_uint64(&current_timestamp, &new_timestamp,
                                 &result_timestamp, enqueuer_rank,
                                 queue->self_rank, queue->min_timestamp_win);
    res = result_timestamp.tag == current_timestamp.tag &&
          result_timestamp.timestamp == current_timestamp.timestamp;
  } else {
    const ltqueue_timestamp_t new_timestamp = {front.timestamp,
                                       current_timestamp.tag + 1};
    ltqueue_timestamp_t result_timestamp;
    compare_and_swap_sync_uint64(&current_timestamp, &new_timestamp,
                                 &result_timestamp, enqueuer_rank,
                                 queue->self_rank, queue->min_timestamp_win);
    res = result_timestamp.tag == current_timestamp.tag &&
          current_timestamp.timestamp == result_timestamp.timestamp;
  }
  return res;
}

static bool _d_refresh_self_node(ltqueue_t *queue, int enqueuer_rank) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  bool res;
  int self_index = _get_enqueuer_index(queue, enqueuer_rank);
  ltqueue_tree_node_t self_node;
  ltqueue_timestamp_t min_timestamp;

  aread_sync(&min_timestamp, sizeof(min_timestamp), enqueuer_rank,
             queue->self_rank, queue->min_timestamp_win);
  aread_sync(&self_node, sizeof(self_node), self_index, queue->self_rank,
             queue->tree_win);

  if (min_timestamp.timestamp == MAX_TIMESTAMP) {
    const ltqueue_tree_node_t new_node = {DUMMY_RANK, self_node.tag + 1};
    ltqueue_tree_node_t result_node;
    compare_and_swap_sync_uint64(&self_node, &new_node, &result_node,
                                 self_index, queue->self_rank, queue->tree_win);
    res =
        result_node.tag == self_node.tag && result_node.rank == self_node.rank;
  } else {
    const ltqueue_tree_node_t new_node = {enqueuer_rank, self_node.tag + 1};
    ltqueue_tree_node_t result_node;
    compare_and_swap_sync_uint64(&self_node, &new_node, &result_node,
                                 self_index, queue->self_rank, queue->tree_win);
    res =
        result_node.tag == self_node.tag && result_node.rank == self_node.rank;
  }
  return res;
}

static bool _d_refresh(ltqueue_t *queue, int current_index) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  ltqueue_tree_node_t current_node;
  uint32_t min_timestamp = MAX_TIMESTAMP;
  int32_t min_timestamp_rank = DUMMY_RANK;

  aread_sync(&current_node, sizeof(current_node), current_index,
             queue->self_rank, queue->tree_win);

  int child_count;
  int *children = _get_children_indexes(queue, current_index, &child_count);

  for (int i = 0; i < child_count; i++) {
    int child_index = children[i];
    ltqueue_tree_node_t child_node;
    aread_sync(&child_node, sizeof(child_node), child_index, queue->self_rank,
               queue->tree_win);

    if (child_node.rank == DUMMY_RANK) {
      continue;
    }

    ltqueue_timestamp_t child_timestamp;
    aread_sync(&child_timestamp, sizeof(child_node), child_node.rank,
               queue->self_rank, queue->min_timestamp_win);

    if (child_timestamp.timestamp < min_timestamp) {
      min_timestamp = child_timestamp.timestamp;
      min_timestamp_rank = child_node.rank;
    }
  }

  const ltqueue_tree_node_t new_node = {min_timestamp_rank, current_node.tag + 1};
  ltqueue_tree_node_t result_node;
  compare_and_swap_sync_uint64(&current_node, &new_node, &result_node,
                               current_index, queue->self_rank,
                               queue->tree_win);
  return result_node.tag == current_node.tag &&
         result_node.rank == current_node.rank;
}

static void _d_propagate(ltqueue_t *queue, int enqueuer_rank) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  if (!_d_refresh_self_node(queue, enqueuer_rank)) {
    _d_refresh_self_node(queue, enqueuer_rank);
  }
  int current_index = _get_enqueuer_index(queue, enqueuer_rank);
  do {
    current_index = _get_parent_index(current_index);
    if (!_d_refresh(queue, current_index)) {
      _d_refresh(queue, current_index);
    }
  } while (current_index != 0);
}

ltqueue_t* ltqueue_init(MPI_Aint capacity_per_node,
                 MPI_Aint dequeuer_rank, MPI_Comm comm) {
  ltqueue_t* queue = malloc(sizeof(ltqueue_t));
  queue->comm = comm;
  queue->dequeuer_rank = dequeuer_rank;
  queue->min_timestamp_win = MPI_WIN_NULL;
  queue->min_timestamp_ptr = NULL;
  queue->tree_win = MPI_WIN_NULL;
  queue->tree_ptr = NULL;
  queue->info = MPI_INFO_NULL;

  MPI_Comm_rank(comm, &queue->self_rank);

  queue->counter = faa_counter_create(dequeuer_rank, comm);

  queue->spsc = spsc_queue_create(capacity_per_node, dequeuer_rank, comm,
                                  sizeof(ltqueue_data_t), 30);

  MPI_Info_create(&queue->info);
  MPI_Info_set(queue->info, "same_disp_unit", "true");
  MPI_Info_set(queue->info, "accumulate_ordering", "none");

  if (queue->self_rank == queue->dequeuer_rank) {
    MPI_Win_allocate(sizeof(ltqueue_timestamp_t) *
                         (_get_number_of_processes(queue) + 1),
                     sizeof(ltqueue_timestamp_t), queue->info, comm,
                     &queue->min_timestamp_ptr, &queue->min_timestamp_win);
    MPI_Win_allocate(_get_tree_size(queue) * sizeof(ltqueue_tree_node_t),
                     sizeof(ltqueue_tree_node_t), queue->info, comm, &queue->tree_ptr,
                     &queue->tree_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->min_timestamp_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->tree_win);

    for (int i = 0; i < _get_tree_size(queue); ++i) {
      queue->tree_ptr[i] = (ltqueue_tree_node_t){DUMMY_RANK, 0};
    }

    const ltqueue_timestamp_t start_timestamp = {MAX_TIMESTAMP, 0};
    for (int i = 0; i < _get_number_of_processes(queue); ++i) {
      awrite_async(&start_timestamp, sizeof(start_timestamp), i,
                   queue->self_rank, queue->min_timestamp_win);
    }
  } else {
    MPI_Win_allocate(0, sizeof(ltqueue_timestamp_t), queue->info, comm,
                     &queue->min_timestamp_ptr, &queue->min_timestamp_win);
    MPI_Win_allocate(0, sizeof(ltqueue_tree_node_t), queue->info, comm,
                     &queue->tree_ptr, &queue->tree_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->min_timestamp_win);
    MPI_Win_lock_all(MPI_MODE_NOCHECK, queue->tree_win);
  }

  MPI_Win_flush_all(queue->min_timestamp_win);
  MPI_Win_flush_all(queue->tree_win);
  MPI_Barrier(comm);
  MPI_Win_flush_all(queue->min_timestamp_win);
  MPI_Win_flush_all(queue->tree_win);

  return queue;
}

void ltqueue_destroy(ltqueue_t *queue) {
  if (queue->min_timestamp_win != MPI_WIN_NULL) {
    MPI_Win_unlock_all(queue->min_timestamp_win);
    MPI_Win_free(&queue->min_timestamp_win);
  }
  if (queue->tree_win != MPI_WIN_NULL) {
    MPI_Win_unlock_all(queue->tree_win);
    MPI_Win_free(&queue->tree_win);
  }
  if (queue->info != MPI_INFO_NULL) {
    MPI_Info_free(&queue->info);
  }

  spsc_queue_destroy(queue->spsc);
  faa_counter_destroy(queue->counter);
}

bool ltqueue_enqueue(ltqueue_t *queue, const char *data, int data_size) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  uint32_t timestamp = faa_counter_get_and_increment(queue->counter);
  ltqueue_data_t timestamped_data = {};
  memcpy(timestamped_data.data, data, data_size);
  timestamped_data.timestamp = timestamp;

  if (!spsc_queue_enqueue(queue->spsc, &timestamped_data)) {
    return false;
  }

  ltqueue_data_t front;
  uint32_t cur_timestamp;
  if (!spsc_queue_e_read_front(queue->spsc, &front)) {
    cur_timestamp = MAX_TIMESTAMP;
  } else {
    cur_timestamp = front.timestamp;
  }

  if (cur_timestamp != timestamp) {
    return true;
  }

  if (!_e_refresh_timestamp(queue)) {
    _e_refresh_timestamp(queue);
  }
  _e_propagate(queue);
  return true;
}

bool ltqueue_dequeue(ltqueue_t *queue, void *output) {
#ifdef PROFILE
  CALI_CXX_MARK_FUNCTION;
#endif
  ltqueue_tree_node_t root;
  aread_sync(&root, sizeof(root), 0, queue->self_rank, queue->tree_win);

  if (root.rank == DUMMY_RANK) {
    return false;
  }

  ltqueue_data_t spsc_output;
  if (!spsc_queue_dequeue(queue->spsc, &spsc_output, root.rank)) {
    return false;
  }

  if (!_d_refresh_timestamp(queue, root.rank)) {
    _d_refresh_timestamp(queue, root.rank);
  }
  _d_propagate(queue, root.rank);
  memcpy(output, spsc_output.data, 512);
  return true;
}
