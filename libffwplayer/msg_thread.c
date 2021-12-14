/******************************************
 * 
 * Simple messaging system across threads
 * 
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 * 
 ******************************************/

#include <stdio.h>
#include <errno.h>
#include "msg_thread.h"
#include "log.h"

#ifdef CK_HANDLE
#define MAGIC 0x4368ae31
#define RETURN_IF_BAD_HANDLE(h,ret) do { \
    if ( ! h) { \
      LOG_E("NULL dst_handle"); \
      return ret; \
    } \
    if (h->magic != MAGIC) { \
      LOG_E("bad dst_handle\n"); \
      return ret; \
    } \
  } while(0)
#else
  #define RETURN_IF_BAD_HANDLE(h,ret) do { /* dummy */ } while(0)
#endif

#define LOCK(m)   pthread_mutex_lock(&m->q_mutex)
#define UNLOCK(m) pthread_mutex_unlock(&m->q_mutex)

typedef struct msg_thread_st * msg_thread_h;  /**< opaque definition for the event thread dst_handle */

#define STACKSIZE__DEFAULT   PTHREAD_STACK_MIN                      /**< 16K set by <limits.h>  (Linux default is 8M) */
#define DEFAULT_SCHED_POLICY SCHED_RR

typedef struct queue_st {
  int max_num_msgs;
  int num_msgs;
  int msg_idx_in;
  int msg_idx_out;
  msg_t * msgs; 
} queue_t;

typedef struct msg_thread_st {
#ifdef CK_HANDLE
  uint32_t        magic;
#endif
  const char *    name;
  pthread_t       tid;
  pthread_mutex_t q_mutex;
  pthread_cond_t  cond;
  queue_t         queue;
} msg_thread_t;

extern int pthread_setname_np(pthread_t thread, const char * name);
extern int pthread_getname_np(pthread_t thread, const char **name);

static bool queue_put( queue_t * q, msg_t * msg)
{
  if (q->num_msgs >= q->max_num_msgs) {
    return false;
  }
  q->msgs[q->msg_idx_in++] = *msg;
  if (q->msg_idx_in >= q->max_num_msgs) {
    q->msg_idx_in = 0;
  }
  q->max_num_msgs++;
  return true;
}

static bool queue_get( queue_t * q, msg_t * msg)
{
  if (q->num_msgs == 0) {
    return false;
  }
  *msg = q->msgs[q->msg_idx_out++];
  if (q->msg_idx_out >= q->max_num_msgs) {
    q->msg_idx_out = 0;
  }
  q->max_num_msgs--;
  return true;
}

msg_thread_h reg_msg_thread(pthread_t id, int max_num_msgs)
{
  msg_thread_t * msg_ht = calloc(1, sizeof(msg_thread_t));
  if ( ! msg_ht) return NULL;
#ifdef CK_HANDLE
  msg_ht->magic = MAGIC;
#endif

  if (pthread_mutex_init(&msg_ht->q_mutex, NULL) != 0) {
    LOG("fail for create mutex\n");
    free(msg_ht);
    return NULL;
  }

  if (pthread_cond_init(&msg_ht->cond, NULL)) {
    pthread_mutex_destroy(&msg_ht->q_mutex);
    LOG("fail for create condvar\n");
    free(msg_ht);
    return NULL;
  }

  msg_ht->queue.msgs = (msg_t *) calloc(max_num_msgs, sizeof(msg_t));
  if ( ! msg_ht->queue.msgs) {
    pthread_cond_destroy(&msg_ht->cond);
    pthread_mutex_destroy(&msg_ht->q_mutex);
    LOG("fail for create queue msgs\n");
    free(msg_ht);
    return NULL;
  }

  msg_ht->tid = id;
  pthread_getname_np(id, &msg_ht->name);

  return msg_ht;
}

bool dereg_msg_thread(msg_thread_h evt_h)
{
  msg_thread_t * msg_ht = (msg_thread_t *) evt_h;
  RETURN_IF_BAD_HANDLE(msg_ht,false);

  free(msg_ht->queue.msgs);
  pthread_cond_destroy(&msg_ht->cond);
  pthread_mutex_destroy(&msg_ht->q_mutex);
  LOG("fail for create queue msgs\n");
  free(msg_ht);
  return NULL;
}

bool post_msg(  msg_thread_h src_handle,
                msg_thread_h dst_handle,
                msg_t * msg)
{
  int i;

  bool broadcast_res = true; // assume all fine for broadcast (if that is the case).
  const char * src_name = "none";

  RETURN_IF_BAD_HANDLE(src_handle, false);

  msg->sender = src_handle;


  LOCK(dst_handle);
  if ( !queue_put(&dst_handle->queue, msg) ) {
    UNLOCK(dst_handle);
    LOG_E("post_event: queue is full \"%s\"", dst_handle->name);
    return false;
  }
  pthread_cond_signal(&dst_handle->cond);
  UNLOCK(dst_handle);
  return true;
}

bool wait_msg(msg_thread_h handle, msg_t * msg_ptr)
{

  RETURN_IF_BAD_HANDLE(handle,false);
  int ret;

  LOCK(handle);
  while ( handle->queue.num_msgs == 0 ) { // queue_get_num_entries tolerates NULL handle
    ret = pthread_cond_wait(&handle->cond, &handle->q_mutex);
    if (ret == ETIMEDOUT) {
      LOG_E("get_event: unexpected timeout");
      continue;
    }
    if (ret) {
      LOG_E("get_event: error %d", ret);
      continue;
    }
  }

  if ( ! queue_get(&handle->queue, msg_ptr)) {
    UNLOCK(handle);
    return false;
  }

  UNLOCK(handle);
  return true;
}

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

msg_thread_h ffw_create_msg_thread( const char * name,
                                    int stackSize,
                                    int priority,
                                    void * ( *thread_entry)(void *),
                                    void * arg,
                                    bool detached,
                                    int msg_queue_size)
{
  msg_thread_h msg_th = NULL;
  pthread_t id = ffw_create_thread( name,
                                    stackSize,
                                    priority,
                                    thread_entry,
                                    arg,
                                    detached);
  if (id == -1) {
    return NULL;
  }
  
  return reg_msg_thread(id, msg_queue_size);

}


