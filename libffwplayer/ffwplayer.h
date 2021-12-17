
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

#define MAX_URL_LEN 512

enum {
  MSG_ID__EOS,
  MSG_ID__PAUSE,
  MSG_ID__RESUME,
  MSG_ID__STOP,
  MSG_ID__SEEK_RELATIVE,
  MSG_ID__POS_REPORT,
  MSG_ID__POS_SET,
  MSG_ID__TIMER
};

typedef struct ffwplayer_st {
  char          url[MAX_URL_LEN + 1];
  msg_thread_h  msg_th;
  msg_thread_h  parent_msg_th;
  void *        private_data;
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

ffwplayer_t * ffw_create_player(char * url, msg_thread_h parent_msg_th, void * client_data);
bool ffw_seek_relative(ffwplayer_t * ffw_t, int val);
bool ffw_destroy(ffwplayer_t * ffw_t);

#ifdef __cplusplus
  }
#endif
