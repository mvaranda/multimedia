/******************************************
 * 
 * Simple messaging system across threads
 * 
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 * 
 ******************************************/

#include <stdio.h>
#include "msg_thread.h"
#include "log.h"

#define MAGIC 0x4368ae31
typedef struct msg_thread_st * msg_thread_h;  /**< opaque definition for the event thread handle */

#define STACKSIZE__DEFAULT   PTHREAD_STACK_MIN                      /**< 16K set by <limits.h>  (Linux default is 8M) */
#define DEFAULT_SCHED_POLICY SCHED_RR

typedef struct queue_st {
  int num_msgs;
  int msg_idx_in;
  int msg_idx_out;
  msg_t * msgs; 
} queue_t;

typedef struct msg_thread_st {
#ifdef CK_HANDLE
  uint32_t        magic;
#endif
  pthread_t       tid;
  pthread_mutex_t q_mutex;
  pthread_cond_t  cond;
  queue_t         queue;
} msg_thread_t;

static queue_t * create_queue(int max_num_msgs)
{
  queue_t * q = calloc( 1, sizeof(queue_t));
  if ( ! q) {
    LOG("fail to alloc memo for queue\n");
    return NULL;
  }
  q->msgs = (msg_t *) calloc(max_num_msgs, sizeof(msg_t));
  if ( ! q->msgs) {
    LOG("fail to alloc memo for msgs\n");
    free(q);
    return NULL;
  }
  return q;
}

msg_thread_h reg_msg_thread(pthread_t id, int max_num_msgs)
{
  msg_thread_t * eh = calloc(1, sizeof(msg_thread_t));
  if ( ! eh) return NULL;
#ifdef CK_HANDLE
  eh->magic = MAGIC;
#endif

  if (pthread_mutex_init(&eh->q_mutex, NULL) != 0) {
    LOG("fail for create mutex\n");
    free(eh);
    return NULL;
  }

  if (pthread_cond_init(&eh->cond, NULL)) {
    pthread_mutex_destroy(&eh->q_mutex);
    LOG("fail for create condvar\n");
    free(eh);
    return NULL;
  }

  eh->queue.msgs = (msg_t *) calloc(max_num_msgs, sizeof(msg_t));
  if ( ! eh->queue.msgs) {
    pthread_cond_destroy(&eh->cond);
    pthread_mutex_destroy(&eh->q_mutex);
    LOG("fail for create queue msgs\n");
    free(eh);
    return NULL;
  }

  eh->tid = id;

  return eh;
}

bool dereg_msg_thread(msg_thread_h evt_h)
{
  msg_thread_t * eh = (msg_thread_t *) evt_h;
#ifdef CK_HANDLE
  if (evt_h->magic != MAGIC) {
    LOG_E("bad handle\n")
    return false;
  }
#endif
  free(eh->queue.msgs);
  pthread_cond_destroy(&eh->cond);
  pthread_mutex_destroy(&eh->q_mutex);
  LOG("fail for create queue msgs\n");
  free(eh);
  return NULL;
}

extern int pthread_setname_np(pthread_t thread, const char * name);

pthread_t ffw_create_thread(const char * name,
                                   int stackSize,
                                   int priority,
                                   void * ( *thread_entry)(void *),
                                   void * arg,
                                   bool detached)
{
  pthread_attr_t attr;
  struct sched_param sch_param;
  pthread_t id;
  int retval = -1;

  pthread_attr_init(&attr);
  // ------------ Set priority ---------
  if (pthread_attr_getschedparam(&attr, &sch_param)) {
    LOG_E("create_event_thread %s: fail to get schedule params", name);
    return retval;
  }
  sch_param.sched_priority = priority;
  if (pthread_attr_setschedpolicy(&attr, DEFAULT_SCHED_POLICY)) {
    LOG_E("create_event_thread %s: fail to set schedule policy", name);
    return retval;
  }
  if (pthread_attr_setschedparam(&attr, &sch_param)) {
    LOG_E("create_event_thread %s: fail to set schedule params", name);
    return retval;
  }

  #ifdef ENABLE_KERNEL_RT
  if (pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED)) {
    LOG_E("create_event_thread %s: fail to set scheduling inheritance", name);
    return retval;
  }
  #endif

  // if stackSize was provided set it
  if (stackSize) {
    if (pthread_attr_setstacksize(&attr, stackSize)) {
      LOG_E("create_event_thread %s: Fail to set stacksize\n", name);
      return retval;
    }
    // maybe otherwise set to STACKSIZE__DEFAULT ?
  }

  if (detached) {
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  }

  int res = pthread_create((pthread_t *)&id, &attr, thread_entry, arg);

  if (res) {
    LOG_E("create_event_thread %s error\n", name);
    return retval;
  }

  pthread_setname_np(id, name);
  return id;
}


