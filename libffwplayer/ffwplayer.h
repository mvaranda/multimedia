
/******************************************
 * 
 * Simple messaging system across threads
 * 
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 * 
 ******************************************/

#pragma once

#include <stdbool.h>
#include "msg_thread.h"

/**
 * @file
 * @brief ffwplayer
 * 
 *
 */

typedef struct ffwplayer_st {
  const char *  url;
  msg_thread_h  msg_th;
  msg_thread_h  parent_msg_th;
  void *        client_data;
} ffwplayer_t;

#ifdef __cplusplus
  extern "C" {
#endif

/**
 * @brief internal function.
 * @warning App should NOT call this function 
 */
void ffwplayer_init(void);

ffwplayer_t * ffw_create_player(const char * url, msg_thread_h parent_msg_th, void * client_data);

#ifdef __cplusplus
  }
#endif
