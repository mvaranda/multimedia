
/**
 *
 * original code from tutorial tutorial07.c
 * "A pedagogical video player that really works! Now with seeking features.""
 *
 * Original tutorial was written by Stephen Dranger (dranger@gmail.com).
 *
 * Code based on FFplay, Copyright (c) 2003 Fabrice Bellard,
 * and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
 * 
 * **********************************************************************
 *   2018 version by Rambod Rahmani
 * 
 *   Ref: https://github.com/rambodrahmani/ffmpeg-video-player
 *
 *   File:   tutorial07.c
 *           This tutorial adds seeking functionalities to the player coded in tutorial06.c
 *
 *           Compiled using
 *               gcc -o ffwplayer ffwplayer.c -lavutil -lavformat -lavcodec -lswscale -lswresample -lz -lm  `sdl2-config --cflags --libs`
 *           on Arch Linux.
 *
 *           Please refer to previous tutorials for uncommented lines of code.
 *
 *   Author: Rambod Rahmani <rambodrahmani@autistici.org>
 *           Created on 8/26/18.
 *
 * *********************************************************************
 *   2021 version by Marcelo Varanda (m@varanda.ca)
 *   - Support for running as a library for both Qt and SDL
 *   - SDL version: window is now resizeble
 *   - WIP: removing dependency on SDL (event and timer already removed). 
 *          for now only audio is using SDL. To be replace (by ALSA or Pulse).
 * 
 *   to compile for SDL just type "make" command. 
 *   to compile for Qt use Qt Creator and open project file under ../ffwdemo/ directory
 * 
 **/

#pragma once

#include <stdbool.h>
#include <libavformat/avformat.h>

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

/**
 * Queue structure used to store processed video frames.
 */
typedef struct VideoPicture {
  AVFrame * frame;
  int width;
  int height;
  int allocated;
  double pts;
} VideoPicture;

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
