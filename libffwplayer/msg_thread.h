/******************************************
 * 
 * Simple messaging system across threads
 * 
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 * 
 ******************************************/
#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

typedef struct msg_thread_st * msg_thread_h;  /**< opaque definition for the event thread handle */

typedef struct msg_st {
  int           msg_id;
  msg_thread_h  sender;
  int           v_int;
  const char *  v_char;
  void *        v_ptr_1;
  void *        v_ptr_2;
  void *        v_ptr_3;
} msg_t;

msg_thread_h reg_msg_thread(pthread_t id, int msg_queue_size);
bool dereg_msg_thread(msg_thread_h evt_h);

bool post_msg(  msg_thread_h src_handle,
                msg_thread_h dst_handle,
                msg_t * msg);

bool wait_msg(msg_thread_h handle, msg_t * msg_ptr);

pthread_t ffw_create_thread(  const char * name,
                              int stackSize,
                              int priority,
                              void * ( *thread_entry)(void *),
                              void * arg,
                              bool detached);

msg_thread_h ffw_create_msg_thread( const char * name,
                                    int stackSize,
                                    int priority,
                                    void * ( *thread_entry)(void *),
                                    void * arg,
                                    bool detached,
                                    int msg_queue_size);
