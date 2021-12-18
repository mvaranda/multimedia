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

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#define _GNU_SOURCE
#include <pthread.h>

#include "ffwplayer.h"
#include "log.h"

#ifdef QT_PLATF
#define USE_RGB32
#endif

#ifdef USE_RGB32
#define AV_VIDEO_FORMAT AV_PIX_FMT_RGB32
#define AV_PIXEL_FORMAT SDL_PIXELFORMAT_RGBA32
#else
#define AV_VIDEO_FORMAT AV_PIX_FMT_YUV420P
#define AV_PIXEL_FORMAT SDL_PIXELFORMAT_YV12
#endif

#define FFWPLAYER_AS_A_LIBRARY
//#define TEST_FFWPLAYER_LIBRARY

/**
 * Prevents SDL from overriding main().
 */
#ifdef __MINGW32__
#undef main
#endif

/**
 * Debug flag.
 */
#define _DEBUG_                       0

#define DEFAULT_WIDTH 800
#define DEFAULT_HEIGTH 500
/**
 * SDL audio buffer size in samples.
 */
#define SDL_AUDIO_BUFFER_SIZE         1024

/**
 * Maximum number of samples per channel in an audio frame.
 */
#define MAX_AUDIO_FRAME_SIZE          192000

/**
 * Audio packets queue maximum size.
 */
#define MAX_AUDIOQ_SIZE               (5 * 16 * 1024)

/**
 * Video packets queue maximum size.
 */
#define MAX_VIDEOQ_SIZE               (5 * 256 * 1024)

/**
 * AV sync correction threshold.
 */
#define AV_SYNC_THRESHOLD             0.01

/**
 * No AV sync correction threshold.
 */
#define AV_NOSYNC_THRESHOLD           1.0

/**
 *
 */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/**
 *
 */
#define AUDIO_DIFF_AVG_NB             20

/**
 * Custom SDL_Event type.
 * Notifies the next video frame has to be displayed.
 */
#define FF_REFRESH_EVENT              (SDL_USEREVENT)

/**
 * Custom SDL_Event type.
 * Notifies the program needs to quit.
 */
#define FF_QUIT_EVENT                 (SDL_USEREVENT + 1)

/**
 * Video Frame queue size.
 */
#define VIDEO_PICTURE_QUEUE_SIZE      1

/**
 * Default audio video sync type.
 */
#define DEFAULT_AV_SYNC_TYPE          AV_SYNC_AUDIO_MASTER

/**
 * Queue structure used to store AVPackets.
 */
typedef struct PacketQueue {
  AVPacketList * first_pkt;
  AVPacketList * last_pkt;
  int nb_packets;
  int size;
  pthread_mutex_t mutex;
  pthread_cond_t cond;

} PacketQueue;

/**
 * Struct used to hold the format context, the indices of the audio and video stream,
 * the corresponding AVStream objects, the audio and video codec information,
 * the audio and video queues and buffers, the global quit flag and the filename of
 * the movie.
 */
typedef struct VideoState {
  /**
   * File I/O Context.
   */
  AVFormatContext * pFormatCtx;

  /**
   * Audio Stream.
   */
  int audioStream;
  AVStream * audio_st;
  AVCodecContext * audio_ctx;
  PacketQueue audioq;
  uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
  unsigned int audio_buf_size;
  unsigned int audio_buf_index;
  AVFrame audio_frame;
  AVPacket audio_pkt;
  uint8_t * audio_pkt_data;
  int audio_pkt_size;
  double audio_clock;

  /**
   * Video Stream.
   */
  int videoStream;
  AVStream * video_st;
  AVCodecContext * video_ctx;
  SDL_Texture * texture;
  SDL_Renderer * renderer;
  PacketQueue videoq;
  struct SwsContext * sws_ctx;
  double frame_timer;
  double frame_last_pts;
  double frame_last_delay;
  double video_clock;
  double video_current_pts;
  int64_t video_current_pts_time;
  double audio_diff_cum;
  double audio_diff_avg_coef;
  double audio_diff_threshold;
  int audio_diff_avg_count;

  /**
   * VideoPicture Queue.
   */
  VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
  int pictq_size;
  int pictq_rindex;
  int pictq_windex;
  pthread_mutex_t pictq_mutex;
  pthread_cond_t pictq_cond;

  /**
   * AV Sync.
   */
  int av_sync_type;
  double external_clock;
  int64_t external_clock_time;

  /**
   * Seeking.
   */
  int seek_req;
  int seek_flags;
  int64_t seek_pos;

  /**
   * Threads.
   */
  pthread_t format_demux_tid;
  pthread_t video_tid;

  /**
   * Input file name.
   */
  char filename[1024];

  /**
   * Global quit flag.
   */
  int quit;

  /**
   * Maximum number of frames to be decoded.
   */
  long maxFramesToDecode;
  int currentFrameIndex;

  SDL_Window * screen;
  pthread_mutex_t screen_mutex;
  // VideoState * global_video_state;
  AVPacket flush_pkt;

  pthread_t video_timer_tid;
  pthread_t audio_thread_tid;
  unsigned int video_timer_delay;
  ffwplayer_t * parent_ffw;

  AVFrame * v_pFrame;
  AVFrame * avFrame;
  //uint8_t * audio_pkt_data;
  //int audio_pkt_size;

} VideoState;

  VideoState * global_video_state;
/**
 * Struct used to hold data fields used for audio resampling.
 */
typedef struct AudioResamplingState {
  SwrContext * swr_ctx;
  int64_t in_channel_layout;
  uint64_t out_channel_layout;
  int out_nb_channels;
  int out_linesize;
  int in_nb_samples;
  int64_t out_nb_samples;
  int64_t max_out_nb_samples;
  uint8_t ** resampled_data;
  int resampled_data_size;

} AudioResamplingState;

/**
 * Audio Video Sync Types.
 */
enum {
  /**
   * Sync to audio clock.
   */
  AV_SYNC_AUDIO_MASTER,

  /**
   * Sync to video clock.
   */
  AV_SYNC_VIDEO_MASTER,

  /**
   * Sync to external clock: the computer clock
   */
  AV_SYNC_EXTERNAL_MASTER,
};

/**
 * Global SDL_Window reference.
 */
// SDL_Window * screen;
// pthread_mutex_t screen_mutex;
// VideoState * global_video_state;
// AVPacket flush_pkt;

/**
 * Methods declaration.
 */
static void printHelpMenu();

static void * format_demux_thread(void * arg);

static int stream_component_open(
  VideoState * videoState,
  int stream_index
  );

static void alloc_picture(void * userdata);

static int queue_picture(
  VideoState * videoState,
  AVFrame * pFrame,
  double pts
  );

static void * video_thread(void * arg);

static int64_t guess_correct_pts(
  AVCodecContext * ctx,
  int64_t reordered_pts,
  int64_t dts
  );

static double synchronize_video(
  VideoState * videoState,
  AVFrame * src_frame,
  double pts
  );

static int synchronize_audio(
  VideoState * videoState,
  short * samples,
  int samples_size
  );

static void video_refresher(void * userdata);

static double get_audio_clock(VideoState * videoState);

static double get_video_clock(VideoState * videoState);

static double get_external_clock(VideoState * videoState);

static double get_master_clock(VideoState * videoState);

static void schedule_refresh(
  VideoState * videoState,
  Uint32 delay
  );

static Uint32 sdl_refresh_timer_cb(
  Uint32 interval,
  void * param
  );

static void video_display(VideoState * videoState);

static void packet_queue_init(PacketQueue * q);

static int packet_queue_put(
  PacketQueue * queue,
  AVPacket * packet
  );

static int packet_queue_get(
  PacketQueue * queue,
  AVPacket * packet,
  int blocking
  );

static void packet_queue_flush(PacketQueue * queue);

void audio_callback(
  void * userdata,
  Uint8 * stream,
  int len
  );

static int audio_decode_frame(
  VideoState * videoState,
  uint8_t * audio_buf,
  int buf_size,
  double * pts_ptr
  );

static int audio_resampling(
  VideoState * videoState,
  AVFrame * decoded_audio_frame,
  enum AVSampleFormat out_sample_fmt,
  uint8_t * out_buf
  );

static void * alsa_audio_thread(void * arg);

static AudioResamplingState * getAudioResampling(uint64_t channel_layout);

static void stream_seek(VideoState * videoState, int64_t pos, int rel);

/**
 * Entry point.
 *
 * @param   argc    command line arguments counter.
 * @param   argv    command line arguments.
 *
 * @return          execution exit code.
 */
// argv[1] has the URL
// argv[2] has number of frames

#ifdef FFWPLAYER_AS_A_LIBRARY



#define GREETINGS "ffwplayer demo - ver 0.0.1\n\n"
#define PROMPT "$ "

extern void update_picture_widget(ffwplayer_t * ffw, VideoPicture * video_picture);

void * ffw_thread(void * arg)
{
  bool ret_bool;
  msg_t msg;

  ffwplayer_t * ffw = (ffwplayer_t *) arg;

  // the global VideoState reference will be set in format_demux_thread() to this pointer
  VideoState * videoState = NULL;

  // allocate memory for the VideoState and zero it out
  videoState = av_mallocz(sizeof(VideoState));
  videoState->parent_ffw = ffw;

  ffw->private_data = videoState;

  videoState->video_timer_tid = -1;
  videoState->audio_thread_tid = -1;
  // set global VideoState reference
  global_video_state = videoState;

  // copy the file name input by the user to the VideoState structure
  av_strlcpy(videoState->filename, ffw->url, sizeof(videoState->filename));

  videoState->maxFramesToDecode = 0; //strtol(ffw->url, &pEnd, 10);


  // initialize locks for the display buffer (pictq)
  pthread_mutex_init(&videoState->pictq_mutex, NULL);
  pthread_cond_init(&videoState->pictq_cond, NULL);

  // launch our threads by pushing an SDL_event of type FF_REFRESH_EVENT
  schedule_refresh(videoState, 100);

  videoState->av_sync_type = DEFAULT_AV_SYNC_TYPE;

  // start the decoding thread to read data from the AVFormatContext
  // videoState->format_demux_tid = SDL_CreateThread(format_demux_thread, "Decoding Thread", videoState);
  videoState->format_demux_tid = ffw_create_thread(
    "format_demux_thread",                                  // name
    0,                                                      // stack size
    20,                                                     // int priority,
    format_demux_thread,                                    // void * ( *thread_entry)(void *),
    videoState,
    true);                                                  // detached

  // check the decode thread was correctly started
  if (videoState->format_demux_tid == -1) {
    LOG_E("Could not start decoding thread.\n");

    // free allocated memory before exiting
    av_free(videoState);

    return NULL;
  }

  av_init_packet(&videoState->flush_pkt);
  videoState->flush_pkt.data = "FLUSH";

  while(1) {
    ret_bool = wait_msg(ffw->msg_th, &msg);
    if ( ret_bool == false) {
      LOG_E("wait_msg error");
      // TODO: free resources
      return NULL;
    }

    switch(msg.msg_id) {
      case MSG_ID__EOS:
        LOG("setting quit flag");
        videoState->quit = 1;
        break;

      case MSG_ID__SEEK_RELATIVE: {
        // LOG("*************** seeking... %d **************", msg.v_int);
        int64_t pos = get_master_clock((VideoState *) ffw->private_data);
        pos += msg.v_int;
        stream_seek((VideoState *) ffw->private_data, (int64_t)(pos * AV_TIME_BASE), msg.v_int);
        }
        break;

      default:
        LOG_W("unhandled message ID= %d", msg.msg_id);
        break;
    }
  }

}

ffwplayer_t * ffw_create_player(char * _url, msg_thread_h parent_msg_th, void * client_data)
{
  ffwplayer_t * ffw;

  if ( ! (ffw = (ffwplayer_t *) calloc(1, sizeof(ffwplayer_t)))) {
    LOG_E("No memo for ffwplayer_t object");
    return NULL;
  }

  //ffw->url = _url;
  //snprintf(ffw->url, MAX_URL_LEN, "%s", _url);
  strcpy(ffw->url, _url);
  ffw->parent_msg_th = parent_msg_th;
  ffw->client_data = client_data;

  ffw->msg_th = ffw_create_msg_thread(
    "ffw_thread",
    0,
    20,                             //   int priority,
    ffw_thread,                     // void * ( *thread_entry)(void *),
    ffw,                            // void * arg,
    true,                           // bool detached,
    6 );                            // int msg_queue_size);


    if ( ! ffw->msg_th) {
     LOG_E("fail to create ffw thread");
     return NULL;  
    }

  return ffw;
}

bool ffw_seek_relative(ffwplayer_t * ffw_t, int val)
{
  msg_t msg;
  msg.msg_id = MSG_ID__SEEK_RELATIVE;
  msg.v_int = val;

  if ( ! post_msg(NULL, ffw_t->msg_th, &msg)) {
    LOG_E("ffw_seek_relative: post error");
    return false;
  }
  return true;
}

bool ffw_destroy(ffwplayer_t * ffw_h)
{
  msg_t msg;
  msg.msg_id = MSG_ID__EOS;
  post_msg(NULL, ffw_h->msg_th, &msg);
  return true;
}

#ifdef TEST_FFWPLAYER_LIBRARY
int main(int argc, char * argv[])
{
  ffwplayer_t * ffw_h;
  msg_thread_h main_msg_th;
  msg_t msg;

  log_init();

  if (argc < 2) {
    LOG_E("missing url argument.");
    return -1;
  }

  /**
   * Initialize SDL.
   * New API: this implementation does not use deprecated SDL functionalities.
   */
  int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
  if (ret != 0) {
    LOG("Could not initialize SDL - %s\n.", SDL_GetError());
    return -1;
  }

  // create a message queue for this main thread
  if ( ! (main_msg_th = reg_msg_thread(pthread_self(), 6)) ) {
    LOG_E("Fail to create msg system for main App thread");
    return -1;
  }

  if ( ! (ffw_h = ffw_create_player(argv[1], main_msg_th, 0))) {
    LOG_E("No memo for ffwplayer_t object");
    return -1;
  }

  char c, line[32];
  int line_idx = 0;
  bool user_quit = false;
  memset(line, 0, sizeof(line));

  printf(GREETINGS);
  printf(PROMPT);
  fflush(stdout);

  while( ! user_quit) {
    scanf("%c", &c);
    if (c != '\n') {
      if (line_idx >= sizeof(line) - 1) {
        line_idx = 0;
        memset(line, 0, sizeof(line));
      }
      line[line_idx++] = c;
      continue;
    }

    switch(line[0]) {
      case 'q':
      case 'Q':
        printf("Exiting...\n");
        // msg.msg_id = MSG_ID__EOS;
        // post_msg(main_msg_th, ffw_h->msg_th, &msg);
        user_quit = true;
        ffw_destroy(ffw_h);
        break;

      case 's':
      {
        char * pEnd;

        int v = strtol(&line[2], &pEnd, 10);

        printf("Seek %d...\n", v);
        // int64_t pos = get_master_clock((VideoState *) ffw_h->private_data);
        // pos += v;
        // stream_seek((VideoState *) ffw_h->private_data, (int64_t)(pos * AV_TIME_BASE), v);
        ffw_seek_relative(ffw_h, v);
        printf(PROMPT);
        fflush(stdout);
        break;
      }
      default:
        printf("invalid option\n\n" PROMPT);
        break;
    }
    line_idx = 0;
    memset(line, 0, sizeof(line));
  } 

  return 0;

}


#endif // TEST_FFWPLAYER_LIBRARY

#else
int main(int argc, char * argv[])
{
  // if the given number of command line arguments is wrong
  if ( argc < 2 ) {
    // print help menu and exit
    printHelpMenu();
    return -1;
  }

  /**
   * Initialize SDL.
   * New API: this implementation does not use deprecated SDL functionalities.
   */
  int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);
  if (ret != 0) {
    LOG("Could not initialize SDL - %s\n.", SDL_GetError());
    return -1;
  }

  // the global VideoState reference will be set in format_demux_thread() to this pointer
  VideoState * videoState = NULL;

  // allocate memory for the VideoState and zero it out
  videoState = av_mallocz(sizeof(VideoState));

  videoState->video_timer_tid = -1;
  // set global VideoState reference
  global_video_state = videoState;

  // copy the file name input by the user to the VideoState structure
  av_strlcpy(videoState->filename, argv[1], sizeof(videoState->filename));


  // parse max frames to decode input by the user
  char * pEnd;
  if (argc >= 3) {
    videoState->maxFramesToDecode = strtol(argv[2], &pEnd, 10);
  }

  // initialize locks for the display buffer (pictq)
  pthread_mutex_init(&videoState->pictq_mutex, NULL);
  pthread_cond_init(&videoState->pictq_cond, NULL);

  // launch our threads by pushing an SDL_event of type FF_REFRESH_EVENT
  schedule_refresh(videoState, 100);

  videoState->av_sync_type = DEFAULT_AV_SYNC_TYPE;

  // start the decoding thread to read data from the AVFormatContext
  // videoState->format_demux_tid = SDL_CreateThread(format_demux_thread, "Decoding Thread", videoState);
  videoState->format_demux_tid = ffw_create_thread(
    "format_demux_thread",                                  // name
    0,                                                      // stack size
    20,                                                     // int priority,
    format_demux_thread,                                    // void * ( *thread_entry)(void *),
    videoState,
    true);                                                  // detached

  // check the decode thread was correctly started
  if (videoState->format_demux_tid == -1) {
    LOG_E("Could not start decoding thread.\n");

    // free allocated memory before exiting
    av_free(videoState);

    return -1;
  }

  av_init_packet(&videoState->flush_pkt);
  videoState->flush_pkt.data = "FLUSH";

#if 1
  char c, line[32];
  int line_idx = 0;
  bool user_quit = false;
  memset(line, 0, sizeof(line));
  while( ! user_quit) {
    scanf("%c", &c);
    if (c != '\n') {
      if (line_idx >= sizeof(line) - 1) {
        line_idx = 0;
        memset(line, 0, sizeof(line));
      }
      line[line_idx++] = c;
      continue;
    }

    switch(line[0]) {
      case 'q':
      case 'Q':
        printf("Exiting...\n");
        user_quit = true;
        break;

      case 's':
      {
        printf("Seek...\n");
        int64_t pos = get_master_clock(global_video_state);
        pos += 60.0;
        stream_seek(global_video_state, (int64_t)(pos * AV_TIME_BASE), 60.0);
        break;
      }
      default:
        printf("invalid option\n");
        break;
    }
    line_idx = 0;
    memset(line, 0, sizeof(line));
  } 
#else
  // infinite loop waiting for fired events
  SDL_Event event;
  for (;;) {
    double incr, pos;

    // wait indefinitely for the next available event
    ret = SDL_WaitEvent(&event);
    if (ret == 0) {
      LOG("SDL_WaitEvent failed: %s.\n", SDL_GetError());
    }

    // switch on the retrieved event type
    switch (event.type) {
      case SDL_KEYDOWN:
      {
        switch (event.key.keysym.sym) {
          case SDLK_LEFT:
          {
            incr = -10.0;
            goto do_seek;
          }
          case SDLK_RIGHT:
          {
            incr = 10.0;
            goto do_seek;
          }
          case SDLK_DOWN:
          {
            incr = -60.0;
            goto do_seek;
          }
          case SDLK_UP:
          {
            incr = 60.0;
            goto do_seek;
          }

 do_seek:
            {
              if (global_video_state) {
                pos = get_master_clock(global_video_state);
                pos += incr;
                stream_seek(global_video_state, (int64_t)(pos * AV_TIME_BASE), incr);
              }
              break;
            };

          default:
          {
            // nothing to do
          }
          break;
        }
      }
      break;

      case FF_QUIT_EVENT:
      case SDL_QUIT:
      {
        videoState->quit = 1;

        /**
         * If the video has finished playing, then both the picture and audio
         * queues are waiting for more data.  Make them stop waiting and
         * terminate normally.
         */
        pthread_cond_signal(&videoState->audioq.cond);
        pthread_cond_signal(&videoState->videoq.cond);

        SDL_Quit();
      }
      break;

      case FF_REFRESH_EVENT:
      {
        video_refresher(event.user.data1);
      }
      break;

      default:
      {
        // nothing to do
      }
      break;
    }

    // check global quit flag
    if (videoState->quit) {
      // exit for loop
      break;
    }
  }
#endif

  // clean up memory
  av_free(videoState);

  return 0;
}
#endif //  FFWPLAYER_AS_A_LIBRARY

/**
 * Print help menu containing usage information.
 */
static void printHelpMenu()
{
  LOG("Invalid arguments.\n\n");
  LOG("Usage: ./ffwplayer filename <max-frames-to-decode>\n\n");
  LOG("e.g: ./ffwplayer $HOME/Videos/video.mp4 200\n");
}

/**
 * This function is used as callback for the SDL_Thread.
 *
 * Opens Audio and Video Streams. If all codecs are retrieved correctly, starts
 * an infinite loop to read AVPackets from the global VideoState AVFormatContext.
 * Based on their stream index, each packet is placed in the appropriate queue.
 *
 * @param   arg the data pointer passed to the SDL_Thread callback function.
 *
 * @return      < 0 in case of error, 0 otherwise.
 */
static void * format_demux_thread(void * arg)
{
  // retrieve global VideoState reference
  VideoState * videoState = (VideoState *)arg;

  // file I/O context: demuxers read a media file and split it into chunks of data (packets)
  AVFormatContext * pFormatCtx = NULL;
  int ret = avformat_open_input(&pFormatCtx, videoState->filename, NULL, NULL);

  if (ret < 0) {
    LOG("Could not open file %s.\n", videoState->filename);
    return (void *)-1;
  }

  // reset stream indexes
  videoState->videoStream = -1;
  videoState->audioStream = -1;

  // // set global VideoState reference
  // global_video_state = videoState;

  // set the AVFormatContext for the global VideoState reference
  videoState->pFormatCtx = pFormatCtx;

  // read packets of the media file to get stream information
  ret = avformat_find_stream_info(pFormatCtx, NULL);
  if (ret < 0) {
    LOG("Could not find stream information: %s.\n", videoState->filename);
    return (void *)-1;
  }

  // dump information about file onto standard error
  if (_DEBUG_) {
    av_dump_format(pFormatCtx, 0, videoState->filename, 0);
  }

  // video and audio stream indexes
  int videoStream = -1;
  int audioStream = -1;

  // loop through the streams that have been found
  for (int i = 0; i < pFormatCtx->nb_streams; i++) {
    // look for the video stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStream < 0) {
      videoStream = i;
    }

    // look for the audio stream
    if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStream < 0) {
      audioStream = i;
    }
  }

  // return with error in case no video stream was found
  if (videoStream == -1) {
    LOG("Could not find video stream.\n");
    goto fail;
  } else {
    // open video stream component codec
    ret = stream_component_open(videoState, videoStream);

    // check video codec was opened correctly
    if (ret < 0) {
      LOG("Could not open video codec.\n");
      goto fail;
    }
  }

  // return with error in case no audio stream was found
  if (audioStream == -1) {
    LOG("Could not find audio stream.\n");
    goto fail;
  } else {
    // open audio stream component codec
    ret = stream_component_open(videoState, audioStream);

    // check audio codec was opened correctly
    if (ret < 0) {
      LOG("Could not open audio codec.\n");
      goto fail;
    }
  }

  // check both the audio and video codecs were correctly retrieved
  if (videoState->videoStream < 0 || videoState->audioStream < 0) {
    LOG("Could not open codecs: %s.\n", videoState->filename);
    goto fail;
  }

  // alloc the AVPacket used to read the media file
  AVPacket * packet = av_packet_alloc();
  if (packet == NULL) {
    LOG("Could not allocate AVPacket.\n");
    goto fail;
  }

  // main decode loop: read in a packet and put it on the right queue
  for (;;) {
    // check global quit flag
    if (videoState->quit) {
      break;
    }

    // seek stuff goes here
    if (videoState->seek_req) {
      int video_stream_index = -1;
      int audio_stream_index = -1;
      int64_t seek_target_video = videoState->seek_pos;
      int64_t seek_target_audio = videoState->seek_pos;

      if (videoState->videoStream >= 0) {
        video_stream_index = videoState->videoStream;
      }

      if (videoState->audioStream >= 0) {
        audio_stream_index = videoState->audioStream;
      }

      if (video_stream_index >= 0 && audio_stream_index >= 0) {
        seek_target_video = av_rescale_q(seek_target_video, AV_TIME_BASE_Q, pFormatCtx->streams[video_stream_index]->time_base);
        seek_target_audio = av_rescale_q(seek_target_audio, AV_TIME_BASE_Q, pFormatCtx->streams[audio_stream_index]->time_base);
      }

      ret = av_seek_frame(videoState->pFormatCtx, video_stream_index, seek_target_video, videoState->seek_flags);
      ret &= av_seek_frame(videoState->pFormatCtx, audio_stream_index, seek_target_audio, videoState->seek_flags);

      if (ret < 0) {
        // LOG_E("%s: error while seeking\n", videoState->pFormatCtx->filename);
        LOG_E("%s: error while seeking\n", videoState->pFormatCtx->url);
      } else {
        if (videoState->videoStream >= 0) {
          packet_queue_flush(&videoState->videoq);
          packet_queue_put(&videoState->videoq, &videoState->flush_pkt);
        }

        if (videoState->audioStream >= 0) {
          packet_queue_flush(&videoState->audioq);
          packet_queue_put(&videoState->audioq, &videoState->flush_pkt);
        }
      }

      videoState->seek_req = 0;
    }

    // check audio and video packets queues size
    if (videoState->audioq.size > MAX_AUDIOQ_SIZE || videoState->videoq.size > MAX_VIDEOQ_SIZE) {
      // wait for audio and video queues to decrease size
      usleep(1000 *  10);

      continue;
    }

    // read data from the AVFormatContext by repeatedly calling av_read_frame()
    ret = av_read_frame(videoState->pFormatCtx, packet);
    if (ret < 0) {
      if (ret == AVERROR_EOF) {
        // media EOF reached, quit
        videoState->quit = 1;
        break;
      } else if (videoState->pFormatCtx->pb->error == 0) {
        // no read error; wait for user input
        usleep(1000 *  10);

        continue;
      } else {
        // exit for loop in case of error
        break;
      }
    }

    // put the packet in the appropriate queue
    if (packet->stream_index == videoState->videoStream) {
      packet_queue_put(&videoState->videoq, packet);
    } else if (packet->stream_index == videoState->audioStream) {
      packet_queue_put(&videoState->audioq, packet);
    } else {
      // otherwise free the memory
      av_packet_unref(packet);
    }
  }

  // wait for the rest of the program to end
  while (!videoState->quit) {
    usleep(1000 *  100);
  }

  // close the opened input AVFormatContext
  avformat_close_input(&pFormatCtx);

  // in case of failure, push the FF_QUIT_EVENT and return
 fail:
  {
    // create an SDL_Event of type FF_QUIT_EVENT
    SDL_Event event;
    event.type = FF_QUIT_EVENT;
    event.user.data1 = videoState;

    // push the event to the events queue
    SDL_PushEvent(&event);

    // return with error
    return (void *)-1;
  };
}

/**
 * Retrieves the AVCodec and initializes the AVCodecContext for the given AVStream
 * index. In case of AVMEDIA_TYPE_AUDIO codec type, it sets the desired audio specs,
 * opens the audio device and starts playing.
 *
 * @param   videoState      the global VideoState reference used to save info
 *                          related to the media being played.
 * @param   stream_index    the stream index obtained from the AVFormatContext.
 *
 * @return                  < 0 in case of error, 0 otherwise.
 */
static int stream_component_open(VideoState * videoState, int stream_index)
{
  //static bool audio_already_open = false;
  // retrieve file I/O context
  AVFormatContext * pFormatCtx = videoState->pFormatCtx;

  // check the given stream index is valid
  if (stream_index < 0 || stream_index >= pFormatCtx->nb_streams) {
    LOG("Invalid stream index.");
    return -1;
  }

  // retrieve codec for the given stream index
  AVCodec * codec = NULL;
  codec = avcodec_find_decoder(pFormatCtx->streams[stream_index]->codecpar->codec_id);
  if (codec == NULL) {
    LOG("Unsupported codec.\n");
    return -1;
  }

  // retrieve codec context
  AVCodecContext * codecCtx = NULL;
  codecCtx = avcodec_alloc_context3(codec);
  int ret = avcodec_parameters_to_context(codecCtx, pFormatCtx->streams[stream_index]->codecpar);
  if (ret != 0) {
    LOG("Could not copy codec context.\n");
    return -1;
  }

  // in case of Audio codec, set up and open the audio device
  if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
#ifdef USE_SDL_AUDIO
    // desired and obtained audio specs references
    SDL_AudioSpec wanted_specs;
    SDL_AudioSpec specs;

    // Set audio settings from codec info
    wanted_specs.freq = codecCtx->sample_rate;
    wanted_specs.format = AUDIO_S16SYS;
    wanted_specs.channels = codecCtx->channels;
    wanted_specs.silence = 0;
    wanted_specs.samples = SDL_AUDIO_BUFFER_SIZE;
    wanted_specs.callback = audio_callback;
    wanted_specs.userdata = videoState;

    /* Deprecated, please refer to tutorial04-resampled.c for the new API */
    // open audio device
    //if (audio_already_open == false) {
      audio_already_open = true;
      ret = SDL_OpenAudio(&wanted_specs, &specs);
      //ret = SDL_OpenAudioDevice(NULL, 0, &wanted_specs, &specs, SDL_AUDIO_ALLOW_ANY_CHANGE);

      // check audio device was correctly opened
      if (ret < 0) {
        LOG("SDL_OpenAudio: %s.\n", SDL_GetError());
        return -1;
      }
    //}
#else // use ALSA 

    // videoState->audio_thread_tid = ffw_create_thread(
    //   "audio_thread",                                         // name
    //   0,                                                      // stack size
    //   20,                                                     // int priority,
    //   alsa_audio_thread,                                      // void * ( *thread_entry)(void *),
    //   videoState,
    //   true);                                                  // detached

#endif // #ifdef USE_SDL_AUDIO
  }


  // initialize the AVCodecContext to use the given AVCodec
  if (avcodec_open2(codecCtx, codec, NULL) < 0) {
    LOG("Unsupported codec.\n");
    return -1;
  }

  // set up the global VideoState based on the type of the codec obtained for
  // the given stream index
  switch (codecCtx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
    {
      // set VideoState audio related fields
      videoState->audioStream = stream_index;
      videoState->audio_st = pFormatCtx->streams[stream_index];
      videoState->audio_ctx = codecCtx;
      videoState->audio_buf_size = 0;
      videoState->audio_buf_index = 0;


      // zero out the block of memory pointed by videoState->audio_pkt
      memset(&videoState->audio_pkt, 0, sizeof(videoState->audio_pkt));

      // init audio packet queue
      packet_queue_init(&videoState->audioq);

      // start playing audio on the first audio device
#ifdef USE_SDL_AUDIO
      SDL_PauseAudio(0);

#else // ALSA
    videoState->audio_thread_tid = ffw_create_thread(
      "audio_thread",                                         // name
      0,                                                      // stack size
      20,                                                     // int priority,
      alsa_audio_thread,                                      // void * ( *thread_entry)(void *),
      videoState,
      true);                                                  // detached
#endif
    }
    break;

    case AVMEDIA_TYPE_VIDEO:
    {
      // set VideoState video related fields
      videoState->videoStream = stream_index;
      videoState->video_st = pFormatCtx->streams[stream_index];
      videoState->video_ctx = codecCtx;

      // Don't forget to initialize the frame timer and the initial
      // previous frame delay: 1ms = 1e-6s
      videoState->frame_timer = (double)av_gettime() / 1000000.0;
      videoState->frame_last_delay = 40e-3;
      videoState->video_current_pts_time = av_gettime();

      // init video packet queue
      packet_queue_init(&videoState->videoq);

      // start video thread
      // videoState->video_tid = SDL_CreateThread(video_thread, "Video Thread", videoState);
      videoState->video_tid =  ffw_create_thread("video_thread",
                                                 0,             // stack size
                                                 20,            // int priority,
                                                 video_thread,  // void * ( *thread_entry)(void *),
                                                 videoState,
                                                 true);         // detached

      // set up the VideoState SWSContext to convert the image data to YUV420
      videoState->sws_ctx = sws_getContext(videoState->video_ctx->width,
                                           videoState->video_ctx->height,
                                           videoState->video_ctx->pix_fmt,
                                           videoState->video_ctx->width,
                                           videoState->video_ctx->height,
                                           AV_VIDEO_FORMAT,
                                           SWS_BILINEAR,
                                           NULL,
                                           NULL,
                                           NULL
                                           );
      //
#ifdef USE_SDL_AUDIO
      SDL_GL_SetSwapInterval(1);
#endif // #ifdef USE_SDL_AUDIO

      // initialize global SDL_Surface mutex reference
      // screen_mutex = SDL_CreateMutex();
      pthread_mutex_init(&videoState->screen_mutex, NULL);

#ifndef QT_PLATF
      // create a window with the specified position, dimensions, and flags.
      videoState->screen = SDL_CreateWindow(
        "FFmpeg SDL Video Player",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        DEFAULT_WIDTH, //codecCtx->width,
        DEFAULT_HEIGTH, //codecCtx->height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
        );

      // check window was correctly created
      if (!videoState->screen) {
        LOG("SDL: could not create window - exiting.\n");
        return -1;
      }



      // create a 2D rendering context for the SDL_Window
      videoState->renderer = SDL_CreateRenderer(videoState->screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);

      // create a texture for a rendering context
      videoState->texture = SDL_CreateTexture(
        videoState->renderer,
        AV_PIXEL_FORMAT,
        SDL_TEXTUREACCESS_STREAMING,
        videoState->video_ctx->width,
        videoState->video_ctx->height
        );
#endif
    }
    break;

    default:
    {
      // nothing to do
    }
    break;
  }

  return 0;
}

/**
 * Allocates a new SDL_Overlay for the VideoPicture struct referenced by the
 * global VideoState struct reference.
 * The remaining VideoPicture struct fields are also updated.
 *
 * @param   userdata    the global VideoState reference.
 */
static void alloc_picture(void * userdata)
{
  // retrieve global VideoState reference.
  VideoState * videoState = (VideoState *)userdata;

  // retrieve the VideoPicture pointed by the queue write index
  VideoPicture * videoPicture;

  videoPicture = &videoState->pictq[videoState->pictq_windex];

  // check if the SDL_Overlay is allocated
  if (videoPicture->frame) {
    // we already have an AVFrame allocated, free memory
    av_frame_free(&videoPicture->frame);
    av_free(videoPicture->frame);
  }

  // lock global screen mutex
  pthread_mutex_lock(&videoState->screen_mutex);

  // get the size in bytes required to store an image with the given parameters
  int numBytes;
  numBytes = av_image_get_buffer_size(
    AV_VIDEO_FORMAT,
    videoState->video_ctx->width,
    videoState->video_ctx->height,
    32
    );

  // allocate image data buffer
  uint8_t * buffer = NULL;
  buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

  // alloc the AVFrame later used to contain the scaled frame
  videoPicture->frame = av_frame_alloc();
  if (videoPicture->frame == NULL) {
    LOG("Could not allocate frame.\n");
    return;
  }

  // The fields of the given image are filled in by using the buffer which points to the image data buffer.
  av_image_fill_arrays(
    videoPicture->frame->data,
    videoPicture->frame->linesize,
    buffer,
    AV_VIDEO_FORMAT,
    videoState->video_ctx->width,
    videoState->video_ctx->height,
    32
    );

  // unlock global screen mutex
  pthread_mutex_unlock(&videoState->screen_mutex);

  // update VideoPicture struct fields
  videoPicture->width = videoState->video_ctx->width;
  videoPicture->height = videoState->video_ctx->height;
  videoPicture->allocated = 1;
}

/**
 * Waits for space in the VideoPicture queue. Allocates a new SDL_Overlay in case
 * it is not already allocated or has a different width/height. Converts the given
 * decoded AVFrame to an AVPicture using specs supported by SDL and writes it in the
 * VideoPicture queue.
 *
 * @param   videoState  the global VideoState reference.
 * @param   pFrame      AVFrame to be inserted in the VideoState->pictq (as an AVPicture).
 *
 * @return              < 0 in case the global quit flag is set, 0 otherwise.
 */
static int queue_picture(VideoState * videoState, AVFrame * pFrame, double pts)
{
  // lock VideoState->pictq mutex
  pthread_mutex_lock(&videoState->pictq_mutex);

  // wait until we have space for a new pic in VideoState->pictq
  while (videoState->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !videoState->quit) {
    pthread_cond_wait(&videoState->pictq_cond, &videoState->pictq_mutex);
  }

  // unlock VideoState->pictq mutex
  pthread_mutex_unlock(&videoState->pictq_mutex);

  // check global quit flag
  if (videoState->quit) {
    return -1;
  }

  // retrieve video picture using the queue write index
  VideoPicture * videoPicture;
  videoPicture = &videoState->pictq[videoState->pictq_windex];

  // if the VideoPicture SDL_Overlay is not allocated or has a different width/height
  if (!videoPicture->frame ||
      videoPicture->width != videoState->video_ctx->width ||
      videoPicture->height != videoState->video_ctx->height) {
    // set SDL_Overlay not allocated
    videoPicture->allocated = 0;

    // allocate a new SDL_Overlay for the VideoPicture struct
    alloc_picture(videoState);

    // check global quit flag
    if (videoState->quit) {
      return -1;
    }
  }

  // check the new SDL_Overlay was correctly allocated
  if (videoPicture->frame) {
    // set pts value for the last decode frame in the VideoPicture queu (pctq)
    videoPicture->pts = pts;

    // set VideoPicture AVFrame info using the last decoded frame
    videoPicture->frame->pict_type = pFrame->pict_type;
    videoPicture->frame->pts = pFrame->pts;
    videoPicture->frame->pkt_dts = pFrame->pkt_dts;
    videoPicture->frame->key_frame = pFrame->key_frame;
    videoPicture->frame->coded_picture_number = pFrame->coded_picture_number;
    videoPicture->frame->display_picture_number = pFrame->display_picture_number;
    videoPicture->frame->width = pFrame->width;
    videoPicture->frame->height = pFrame->height;

    // scale the image in pFrame->data and put the resulting scaled image in pict->data
    sws_scale(
      videoState->sws_ctx,
      (uint8_t const * const *)pFrame->data,
      pFrame->linesize,
      0,
      videoState->video_ctx->height,
      videoPicture->frame->data,
      videoPicture->frame->linesize
      );

    // update VideoPicture queue write index
    ++videoState->pictq_windex;

    // if the write index has reached the VideoPicture queue size
    if (videoState->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
      // set it to 0
      videoState->pictq_windex = 0;
    }

    // lock VideoPicture queue
    pthread_mutex_lock(&videoState->pictq_mutex);

    // increase VideoPicture queue size
    videoState->pictq_size++;

    // unlock VideoPicture queue
    pthread_mutex_unlock(&videoState->pictq_mutex);
  }

  return 0;
}

/**
 * This function is used as callback for the SDL_Thread.
 *
 * This thread reads in packets from the video queue, packet_queue_get(), decodes
 * the video packets into a frame, and then calls the queue_picture() function to
 * put the processed frame into the picture queue.
 *
 * @param   arg the data pointer passed by the thread callback function.
 *
 * @return
 */
static void * video_thread(void * arg)
{
  // retrieve global VideoState reference
  VideoState * videoState = (VideoState *)arg;

  // allocate an AVPacket to be used to retrieve data from the videoq.
  AVPacket * packet = av_packet_alloc();

  if (packet == NULL) {
    LOG("Could not allocate AVPacket.\n");
    return (void *)-1;
  }

  // set this when we are done decoding an entire frame
  int frameFinished = 0;

  // allocate a new AVFrame, used to decode video packets
  videoState->v_pFrame = av_frame_alloc();
  if (!videoState->v_pFrame) {
    LOG("Could not allocate AVFrame.\n");
    return (void *)-1;
  }

  // each decoded frame carries its PTS in the VideoPicture queue
  double pts;

  for (;;) {
    // get a packet from the video PacketQueue
    int ret = packet_queue_get(&videoState->videoq, packet, 1);
    if (ret < 0) {
      // means we quit getting packets
      break;
    }

    if (packet->data == videoState->flush_pkt.data) {
      avcodec_flush_buffers(videoState->video_ctx);
      continue;
    }

    // give the decoder raw compressed data in an AVPacket
    ret = avcodec_send_packet(videoState->video_ctx, packet);
    if (ret < 0) {
      LOG("Error sending packet for decoding.\n");
      return (void *)-1;
    }

    while (ret >= 0) {
      // get decoded output data from decoder
      ret = avcodec_receive_frame(videoState->video_ctx, videoState->v_pFrame);

      // check an entire frame was decoded
      if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        break;
      } else if (ret < 0) {
        LOG("Error while decoding.\n");
        return (void *)-1;
      } else {
        frameFinished = 1;
      }

      // attempt to guess proper monotonic timestamps for decoded video frames
      pts = guess_correct_pts(videoState->video_ctx, videoState->v_pFrame->pts, videoState->v_pFrame->pkt_dts);

      // in case we get an undefined timestamp value
      if (pts == AV_NOPTS_VALUE) {
        // set pts to the default value of 0
        pts = 0;
      }

      pts *= av_q2d(videoState->video_st->time_base);

      // did we get an entire video frame?
      if (frameFinished) {
        pts = synchronize_video(videoState, videoState->v_pFrame, pts);

        if (queue_picture(videoState, videoState->v_pFrame, pts) < 0) {
          break;
        }
      }
    }

    // wipe the packet
    av_packet_unref(packet);
  }

  // wipe the frame
  av_frame_free(&videoState->v_pFrame);
  av_free(videoState->v_pFrame);

  return 0;
}

/**
 * Attempts to guess proper monotonic timestamps for decoded video frames which
 * might have incorrect times.
 *
 * Input timestamps may wrap around, in which case the output will as well.
 *
 * @param   ctx             the video AVCodecContext.
 * @param   reordered_pts   the pts field of the decoded AVPacket, as passed
 *                          through AVFrame.pts.
 * @param   dts             the pkt_dts field of the decoded AVPacket.
 *
 * @return                  one of the input values, may be AV_NOPTS_VALUE.
 */
static int64_t guess_correct_pts(AVCodecContext * ctx, int64_t reordered_pts, int64_t dts)
{
  int64_t pts;

  if (dts != AV_NOPTS_VALUE) {
    ctx->pts_correction_num_faulty_dts += dts <= ctx->pts_correction_last_dts;
    ctx->pts_correction_last_dts = dts;
  } else if (reordered_pts != AV_NOPTS_VALUE) {
    ctx->pts_correction_last_dts = reordered_pts;
  }

  if (reordered_pts != AV_NOPTS_VALUE) {
    ctx->pts_correction_num_faulty_pts += reordered_pts <= ctx->pts_correction_last_pts;
    ctx->pts_correction_last_pts = reordered_pts;
  } else if (dts != AV_NOPTS_VALUE) {
    ctx->pts_correction_last_pts = dts;
  }

  if ((ctx->pts_correction_num_faulty_pts <= ctx->pts_correction_num_faulty_dts || dts == AV_NOPTS_VALUE) && reordered_pts != AV_NOPTS_VALUE) {
    pts = reordered_pts;
  } else {
    pts = dts;
  }

  return pts;
}

/**
 * Updates the PTS of the last decoded video frame to be in sync with everything.
 * This function will also deal with cases where we don't get a PTS value
 * for our frame. At the same time we need to keep track of when the next frame
 * is expected so we can set our refresh rate properly. We can accomplish this by
 * using the VideoState internal video_clock value which keeps track of how much
 * time has passed according to the video.
 *
 * You'll notice we account for repeated frames in this function, too.
 *
 * @param   videoState  the global VideoState reference.
 * @param   src_frame   last decoded video AVFrame, not yet queued in the
 *                      VideoPicture queue.
 * @param   pts         the pts of the last decoded video AVFrame obtained using
 *                      FFmpeg guess_correct_pts.
 *
 * @return              the updated (synchronized) pts for the given video AVFrame.
 */
static double synchronize_video(VideoState * videoState, AVFrame * src_frame, double pts)
{
  double frame_delay;

  if (pts != 0) {
    // if we have pts, set video clock to it
    videoState->video_clock = pts;
  } else {
    // if we aren't given a pts, set it to the clock
    pts = videoState->video_clock;
  }

  // update the video clock
  frame_delay = av_q2d(videoState->video_ctx->time_base);

  // if we are repeating a frame, adjust clock accordingly
  frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);

  // increase video clock to match the delay required for repeaing frames
  videoState->video_clock += frame_delay;

  return pts;
}

/**
 * So we're going to use a fractional coefficient, say c, and So now let's say
 * we've gotten N audio sample sets that have been out of sync. The amount we are
 * out of sync can also vary a good deal, so we're going to take an average of how
 * far each of those have been out of sync. So for example, the first call might
 * have shown we were out of sync by 40ms, the next by 50ms, and so on. But we're
 * not going to take a simple average because the most recent values are more
 * important than the previous ones. So we're going to use a fractional coefficient,
 * say c, and sum the differences like this: diff_sum = new_diff + diff_sum*c.
 * When we are ready to find the average difference, we simply calculate
 * avg_diff = diff_sum * (1-c).
 *
 * @param   videoState      the global VideoState reference.
 * @param   samples         global VideoState reference audio buffer.
 * @param   samples_size    last decoded audio AVFrame size after resampling.
 *
 * @return
 */
static int synchronize_audio(VideoState * videoState, short * samples, int samples_size)
{
  int n;
  double ref_clock;

  n = 2 * videoState->audio_ctx->channels;

  // check if
  if (videoState->av_sync_type != AV_SYNC_AUDIO_MASTER) {
    double diff, avg_diff;
    int wanted_size, min_size, max_size /*, nb_samples */;

    ref_clock = get_master_clock(videoState);
    diff = get_audio_clock(videoState) - ref_clock;

    if (diff < AV_NOSYNC_THRESHOLD) {
      // accumulate the diffs
      videoState->audio_diff_cum = diff + videoState->audio_diff_avg_coef * videoState->audio_diff_cum;

      if (videoState->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
        videoState->audio_diff_avg_count++;
      } else {
        avg_diff = videoState->audio_diff_cum * (1.0 - videoState->audio_diff_avg_coef);

        /**
         * So we're doing pretty well; we know approximately how off the audio
         * is from the video or whatever we're using for a clock. So let's now
         * calculate how many samples we need to add or lop off by putting this
         * code where the "Shrinking/expanding buffer code" section is:
         */
        if (fabs(avg_diff) >= videoState->audio_diff_threshold) {
          wanted_size = samples_size + ((int)(diff * videoState->audio_ctx->sample_rate) * n);
          min_size = samples_size * ((100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100);
          max_size = samples_size * ((100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100);

          if (wanted_size < min_size) {
            wanted_size = min_size;
          } else if (wanted_size > max_size) {
            wanted_size = max_size;
          }

          /**
           * Now we have to actually correct the audio. You may have noticed that our
           * synchronize_audio function returns a sample size, which will then tell us
           * how many bytes to send to the stream. So we just have to adjust the sample
           * size to the wanted_size. This works for making the sample size smaller.
           * But if we want to make it bigger, we can't just make the sample size larger
           * because there's no more data in the buffer! So we have to add it. But what
           * should we add? It would be foolish to try and extrapolate audio, so let's
           * just use the audio we already have by padding out the buffer with the
           * value of the last sample.
           */
          if (wanted_size < samples_size) {
            /* remove samples */
            samples_size = wanted_size;
          } else if (wanted_size > samples_size) {
            uint8_t * samples_end, * q;
            int nb;

            /* add samples by copying final sample*/
            nb = (samples_size - wanted_size);
            samples_end = (uint8_t *)samples + samples_size - n;
            q = samples_end + n;

            while (nb > 0) {
              memcpy(q, samples_end, n);
              q += n;
              nb -= n;
            }

            samples_size = wanted_size;
          }
        }
      }
    } else {
      /* difference is TOO big; reset diff stuff */
      videoState->audio_diff_avg_count = 0;
      videoState->audio_diff_cum = 0;
    }
  }

  return samples_size;
}

/**
 * Pulls from the VideoPicture queue when we have something, sets our timer for
 * when the next video frame should be shown, calls the video_display() method to
 * actually show the video on the screen, then decrements the counter on the queue,
 * and decreases its size.
 *
 * @param   userdata    SDL_UserEvent->data1;   User defined data pointer.
 */
static void video_refresher(void * userdata)
{
  // LOG_E("\n!!!VIDEO_REFRESH_TIMER CALLED!!!\n");

  // retrieve global VideoState reference
  VideoState * videoState = (VideoState *)userdata;

  // VideoPicture read index reference
  VideoPicture * videoPicture;

  // used for video frames display delay and audio video sync
  double pts_delay;
  double audio_ref_clock;
  double sync_threshold;
  double real_delay;
  double audio_video_delay;

  // check the video stream was correctly opened
  if (videoState->video_st) {
    // check the VideoPicture queue contains decoded frames
    if (videoState->pictq_size == 0) {
      //LOG_E("\n!!!videoState->pictq_size == 0!!!\n");

      schedule_refresh(videoState, 1);
    } else {
      // get VideoPicture reference using the queue read index
      videoPicture = &videoState->pictq[videoState->pictq_rindex];

      if (_DEBUG_) {
        LOG("Current Frame PTS:\t\t%f\n", videoPicture->pts);
        LOG("Last Frame PTS:\t\t\t%f\n",  videoState->frame_last_pts);
      }

      // get last frame pts
      pts_delay = videoPicture->pts - videoState->frame_last_pts;

      if (_DEBUG_) {
        LOG("PTS Delay:\t\t\t\t%f\n", pts_delay);
      }

      // if the obtained delay is incorrect
      if (pts_delay <= 0 || pts_delay >= 1.0) {
        // use the previously calculated delay
        pts_delay = videoState->frame_last_delay;
      }

      if (_DEBUG_) {
        LOG("Corrected PTS Delay:\t%f\n", pts_delay);
      }

      // save delay information for the next time
      videoState->frame_last_delay = pts_delay;
      videoState->frame_last_pts = videoPicture->pts;

      // in case the external clock is not used
      if (videoState->av_sync_type != AV_SYNC_VIDEO_MASTER) {
        // update delay to stay in sync with the master clock: audio or video
        audio_ref_clock = get_master_clock(videoState);

        if (_DEBUG_) {
          LOG("Ref Clock:\t\t\t\t%f\n", audio_ref_clock);
        }

        // calculate audio video delay accordingly to the master clock
        audio_video_delay = videoPicture->pts - audio_ref_clock;

        if (_DEBUG_) {
          LOG("Audio Video Delay:\t\t%f\n", audio_video_delay);
        }

        // skip or repeat the frame taking into account the delay
        sync_threshold = (pts_delay > AV_SYNC_THRESHOLD) ? pts_delay : AV_SYNC_THRESHOLD;

        if (_DEBUG_) {
          LOG("Sync Threshold:\t\t\t%f\n", sync_threshold);
        }

        // check audio video delay absolute value is below sync threshold
        if (fabs(audio_video_delay) < AV_NOSYNC_THRESHOLD) {
          if (audio_video_delay <= -sync_threshold) {
            pts_delay = 0;
          } else if (audio_video_delay >= sync_threshold) {
            pts_delay = 2 * pts_delay;
          }
        }
      }

      if (_DEBUG_) {
        LOG("Corrected PTS delay:\t%f\n", pts_delay);
      }

      videoState->frame_timer += pts_delay;

      // compute the real delay
      real_delay = videoState->frame_timer - (av_gettime() / 1000000.0);

      if (_DEBUG_) {
        LOG("Real Delay:\t\t\t\t%f\n", real_delay);
      }

      if (real_delay < 0.010) {
        real_delay = 0.010;
      }

      if (_DEBUG_) {
        LOG("Corrected Real Delay:\t%f\n", real_delay);
      }

      schedule_refresh(videoState, (Uint32)(real_delay * 1000 + 0.5));

      if (_DEBUG_) {
        LOG("Next Scheduled Refresh:\t%f\n\n", (real_delay * 1000 + 0.5));
      }

      // show the frame on the SDL_Surface (the screen)
      video_display(videoState);

      // update read index for the next frame
      if (++videoState->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
        videoState->pictq_rindex = 0;
      }

      // lock VideoPicture queue mutex
      pthread_mutex_lock(&videoState->pictq_mutex);

      // decrease VideoPicture queue size
      videoState->pictq_size--;

      // notify other threads waiting for the VideoPicture queue
      pthread_cond_signal(&videoState->pictq_cond);

      // unlock VideoPicture queue mutex
      pthread_mutex_unlock(&videoState->pictq_mutex);
    }
  } else {
    schedule_refresh(videoState, 100);
  }
}

/**
 * Calculates and returns the current audio clock reference value.
 *
 * @param   videoState  the global VideoState reference.
 *
 * @return              the current audio clock reference value.
 */
static double get_audio_clock(VideoState * videoState)
{
  double pts = videoState->audio_clock;

  int hw_buf_size = videoState->audio_buf_size - videoState->audio_buf_index;

  int bytes_per_sec = 0;

  int n = 2 * videoState->audio_ctx->channels;

  if (videoState->audio_st) {
    bytes_per_sec = videoState->audio_ctx->sample_rate * n;
  }

  if (bytes_per_sec) {
    pts -= (double)hw_buf_size / bytes_per_sec;
  }

  return pts;
}

/**
 * Calculates and returns the current video clock reference value.
 *
 * @param   videoState  the global VideoState reference.
 *
 * @return              the current video clock reference value.
 */
static double get_video_clock(VideoState * videoState)
{
  double delta = (av_gettime() - videoState->video_current_pts_time) / 1000000.0;

  return videoState->video_current_pts + delta;
}

/**
 * Calculates and returns the current external clock reference value: the computer
 * clock.
 *
 * @return  the current external clock reference value.
 */
static double get_external_clock(VideoState * videoState)
{
  videoState->external_clock_time = av_gettime();
  videoState->external_clock = videoState->external_clock_time / 1000000.0;

  return videoState->external_clock;
}

/**
 * Checks the VideoState global reference av_sync_type variable and then calls
 * get_audio_clock, get_video_clock, or get_external_clock accordingly.
 *
 * @param   videoState  the global VideoState reference.
 *
 * @return              the reference clock according to the chosen AV sync type.
 */
static double get_master_clock(VideoState * videoState)
{
  if (videoState->av_sync_type == AV_SYNC_VIDEO_MASTER) {
    return get_video_clock(videoState);
  } else if (videoState->av_sync_type == AV_SYNC_AUDIO_MASTER) {
    return get_audio_clock(videoState);
  } else if (videoState->av_sync_type == AV_SYNC_EXTERNAL_MASTER) {
    return get_external_clock(videoState);
  } else {
    LOG_E("Error: Undefined A/V sync type.");
    return -1;
  }
}

void * video_timer_thread(void * arg)
{
  VideoState * videoState = (VideoState *) arg;

  while(1) {
    if (global_video_state->quit) {
      break;
    }
    if (videoState->video_timer_delay == 0) {
      usleep(1000);
      //continue;
    }
    
    usleep(videoState->video_timer_delay * 1000);
    video_refresher(videoState); 
  }
}

/**
 * Schedules video updates - every time we call this function, it will set the
 * timer, which will trigger an event, which will have our main() function in turn
 * call a function that pulls a frame from our picture queue and displays it.
 *
 * @param   videoState  the global VideoState reference.
 * @param   delay       the delay, expressed in milliseconds, before displaying
 *                      the next video frame on the screen.
 */
static void schedule_refresh(VideoState * videoState, Uint32 delay)
{
#if 1
  videoState->video_timer_delay = delay;
  if (videoState->video_timer_tid == -1) {
    videoState->video_timer_tid = ffw_create_thread(
      "format_demux_thread",                                  // name
      0,                                                      // stack size
      20,                                                     // int priority,
      video_timer_thread,                                     // void * ( *thread_entry)(void *),
      videoState,
      true);                                                  // detached
  }

#else
  // schedule an SDL timer
  int ret = SDL_AddTimer(delay, sdl_refresh_timer_cb, videoState);

  // check the timer was correctly scheduled
  if (ret == 0) {
    LOG("Could not schedule refresh callback: %s.\n.", SDL_GetError());
  }
#endif
}

/**
 * This is the callback function for the SDL Timer.
 *
 * Pushes an SDL_Event of type FF_REFRESH_EVENT to the events queue.
 *
 * @param   interval    the timer delay in milliseconds.
 * @param   param       user defined data passed to the callback function when
 *                      scheduling the timer. In our case the global VideoState
 *                      reference.
 *
 * @return              if the returned value from the callback is 0, the timer
 *                      is canceled.
 */
static Uint32 sdl_refresh_timer_cb(Uint32 interval, void * param)
{
  // create an SDL_Event of type FF_REFRESH_EVENT
  SDL_Event event;

  event.type = FF_REFRESH_EVENT;
  event.user.data1 = param;

  // push the event to the events queue
  SDL_PushEvent(&event);

  // return 0 to cancel the timer
  return 0;
}

/**
 * Retrieves the video aspect ratio first, which is just the width divided by the
 * height. Then it scales the movie to fit as big as possible in our screen
 * (SDL_Surface). Then it centers the movie, and calls SDL_DisplayYUVOverlay()
 * to update the surface, making sure we use the screen mutex to access it.
 *
 * @param   videoState  the global VideoState reference.
 */
static void video_display(VideoState * videoState)
{
  // reference for the next VideoPicture to be displayed
  VideoPicture * videoPicture;

  double aspect_ratio;

  int w, h, x, y;

  // get next VideoPicture to be displayed from the VideoPicture queue
  videoPicture = &videoState->pictq[videoState->pictq_rindex];

  if (videoPicture->frame) {
    if (videoState->video_ctx->sample_aspect_ratio.num == 0) {
      aspect_ratio = 0;
    } else {
      aspect_ratio = av_q2d(videoState->video_ctx->sample_aspect_ratio) * videoState->video_ctx->width / videoState->video_ctx->height;
    }

    if (aspect_ratio <= 0.0) {
      aspect_ratio = (float)videoState->video_ctx->width /
        (float)videoState->video_ctx->height;
    }

    // get the size of a window's client area
    int screen_width;
    int screen_height;

#ifdef QT_PLATF
    update_picture_widget(videoState->parent_ffw, videoPicture);
#else
    // MV: work around to get window resize updating its internal width and heigth
    SDL_Event ev;
    while(SDL_PollEvent(&ev)) { /* summy */ }

    SDL_GetWindowSize(videoState->screen, &screen_width, &screen_height);

    // global SDL_Surface height
    h = screen_height;

    // retrieve width using the calculated aspect ratio and the screen height
    w = ((int)rint(h * aspect_ratio)) & -3;

    // if the new width is bigger than the screen width
    if (w > screen_width) {
      // set the width to the screen width
      w = screen_width;

      // recalculate height using the calculated aspect ratio and the screen width
      h = ((int)rint(w / aspect_ratio)) & -3;
    }

    // TODO: Add full screen support
    x = (screen_width - w);
    y = (screen_height - h);

    // check the number of frames to decode was not exceeded
    videoState->currentFrameIndex++;
    if ((videoState->maxFramesToDecode == 0) || 
      (videoState->currentFrameIndex < videoState->maxFramesToDecode)) {
//    if (videoState->currentFrameIndex < videoState->maxFramesToDecode) {
      if (_DEBUG_) {
        // dump information about the frame being rendered
        printf(
          "Frame %c (%d) pts %" PRId64 " dts %" PRId64 " key_frame %d [coded_picture_number %d, display_picture_number %d, %dx%d]\n",
          av_get_picture_type_char(videoPicture->frame->pict_type),
          videoState->video_ctx->frame_number,
          videoPicture->frame->pts,
          videoPicture->frame->pkt_dts,
          videoPicture->frame->key_frame,
          videoPicture->frame->coded_picture_number,
          videoPicture->frame->display_picture_number,
          videoPicture->frame->width,
          videoPicture->frame->height
          );
      }

      SDL_Rect rect_picture;
      rect_picture.x = 0;
      rect_picture.y = 0;
      rect_picture.w = videoState->video_ctx->width;
      rect_picture.h = videoState->video_ctx->height;


      SDL_Rect rect_win;
      rect_win.w = w;
      rect_win.h = h;
      rect_win.x = 0;
      if (screen_width > w) {
        rect_win.x = (screen_width - w) / 2;
      }
      rect_win.y = 0;
      if (screen_height > h) {
        rect_win.y = (screen_height - h ) / 2;
      }


      // lock screen mutex
      pthread_mutex_lock(&videoState->screen_mutex);

      // update the texture with the new pixel data
#ifdef USE_RGB32
      SDL_UpdateTexture(
        videoState->texture,
        &rect_picture, // &rect,
        videoPicture->frame->data[0],
        *videoPicture->frame->linesize);
#else
      SDL_UpdateYUVTexture(
        videoState->texture,
        &rect_picture, // &rect,
        videoPicture->frame->data[0],
        videoPicture->frame->linesize[0],
        videoPicture->frame->data[1],
        videoPicture->frame->linesize[1],
        videoPicture->frame->data[2],
        videoPicture->frame->linesize[2]
        );
#endif
      // clear the current rendering target with the drawing color
      SDL_RenderClear(videoState->renderer);

      // copy a portion of the texture to the current rendering target
      SDL_RenderCopy(videoState->renderer, videoState->texture, NULL, &rect_win);

      // update the screen with any rendering performed since the previous call
      SDL_RenderPresent(videoState->renderer);

      // unlock screen mutex
      pthread_mutex_unlock(&videoState->screen_mutex);
    } else {
      // create an SDLEvent of type FF_QUIT_EVENT
      SDL_Event event;
      event.type = FF_QUIT_EVENT;
      event.user.data1 = videoState;

      // push the event
      SDL_PushEvent(&event);
    }
#endif // SDL version
  }
}

/**
 * Initialize the given PacketQueue.
 *
 * @param q the PacketQueue to be initialized.
 */
static void packet_queue_init(PacketQueue * q)
{
  // alloc memory for the audio queue
  memset(
    q,
    0,
    sizeof(PacketQueue)
    );

  // Returns the initialized and unlocked mutex or NULL on failure
  // q->mutex = SDL_CreateMutex();
  // if (!q->mutex)
  if (pthread_mutex_init(&q->mutex, NULL) != 0) {
    // could not create mutex
    LOG("SDL_CreateMutex Error: %s.\n", SDL_GetError());
    return;
  }

  // Returns a new condition variable or NULL on failure
  // q->cond = SDL_CreateCond();
  // if (!q->cond)
  if (pthread_cond_init(&q->cond, NULL) != 0) {
    // could not create condition variable
    LOG("SDL_CreateCond Error: %s.\n", SDL_GetError());
    return;
  }
}

/**
 * Put the given AVPacket in the given PacketQueue.
 *
 * @param  queue    the queue to be used for the insert
 * @param  packet   the AVPacket to be inserted in the queue
 *
 * @return          0 if the AVPacket is correctly inserted in the given PacketQueue.
 */
static int packet_queue_put(PacketQueue * queue, AVPacket * packet)
{
  // alloc the new AVPacketList to be inserted in the audio PacketQueue
  AVPacketList * avPacketList;

  avPacketList = av_malloc(sizeof(AVPacketList));

  // check the AVPacketList was allocated
  if (!avPacketList) {
    return -1;
  }

  // add reference to the given AVPacket
  avPacketList->pkt = *packet;

  // the new AVPacketList will be inserted at the end of the queue
  avPacketList->next = NULL;

  // lock mutex
  pthread_mutex_lock(&queue->mutex);

  // check the queue is empty
  if (!queue->last_pkt) {
    // if it is, insert as first
    queue->first_pkt = avPacketList;
  } else {
    // if not, insert as last
    queue->last_pkt->next = avPacketList;
  }

  // point the last AVPacketList in the queue to the newly created AVPacketList
  queue->last_pkt = avPacketList;

  // increase by 1 the number of AVPackets in the queue
  queue->nb_packets++;

  // increase queue size by adding the size of the newly inserted AVPacket
  queue->size += avPacketList->pkt.size;

  // notify packet_queue_get which is waiting that a new packet is available
  pthread_cond_signal(&queue->cond);

  // unlock mutex
  pthread_mutex_unlock(&queue->mutex);

  return 0;
}

/**
 * Get the first AVPacket from the given PacketQueue.
 *
 * @param   queue       the PacketQueue to extract from.
 * @param   packet      the first AVPacket extracted from the queue.
 * @param   blocking    0 to avoid waiting for an AVPacket to be inserted in the given
 *                      queue, != 0 otherwise.
 *
 * @return              < 0 if returning because the quit flag is set, 0 if the queue
 *                      is empty, 1 if it is not empty and a packet was extracted.
 */
static int packet_queue_get(PacketQueue * queue, AVPacket * packet, int blocking)
{
  int ret;

  AVPacketList * avPacketList;

  // lock mutex
  pthread_mutex_lock(&queue->mutex);

  for (;;) {
    // check quit flag
    if (global_video_state->quit) {
      ret = -1;
      break;
    }

    // point to the first AVPacketList in the queue
    avPacketList = queue->first_pkt;

    // if the first packet is not NULL, the queue is not empty
    if (avPacketList) {
      // place the second packet in the queue at first position
      queue->first_pkt = avPacketList->next;

      // check if queue is empty after removal
      if (!queue->first_pkt) {
        // first_pkt = last_pkt = NULL = empty queue
        queue->last_pkt = NULL;
      }

      // decrease the number of packets in the queue
      queue->nb_packets--;

      // decrease the size of the packets in the queue
      queue->size -= avPacketList->pkt.size;

      // point packet to the extracted packet, this will return to the calling function
      *packet = avPacketList->pkt;

      // free memory
      av_free(avPacketList);

      ret = 1;
      break;
    } else if (!blocking) {
      ret = 0;
      break;
    } else {
      // unlock mutex and wait for cond signal, then lock mutex again
      pthread_cond_wait(&queue->cond, &queue->mutex);
    }
  }

  // unlock mutex
  pthread_mutex_unlock(&queue->mutex);

  return ret;
}

/**
 *
 * @param queue
 */
static void packet_queue_flush(PacketQueue * queue)
{
  AVPacketList * pkt, * pkt1;

  pthread_mutex_lock(&queue->mutex);

  for (pkt = queue->first_pkt; pkt != NULL; pkt = pkt1) {
    pkt1 = pkt->next;
    // av_free_packet(&pkt->pkt);
    av_packet_unref(&pkt->pkt);

    av_freep(&pkt);
  }

  queue->last_pkt = NULL;
  queue->first_pkt = NULL;
  queue->nb_packets = 0;
  queue->size = 0;

  pthread_mutex_unlock(&queue->mutex);
}


/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
static void * alsa_audio_thread(void * arg)
{
  struct timespec tm;

  int rc;
  int size;
  snd_pcm_t *handle;
  snd_pcm_hw_params_t *params;
  unsigned int val;
  int dir;
  snd_pcm_uframes_t frames;
  char *buffer;
  tm.tv_sec = 0;
  tm.tv_nsec = 50000000 / 2;

  Uint8 buf[2205];
  //50ms @ 44100 = 4410 samples... *  4 bytes per sample = 2205 bytes
  LOG("alsa_audio_thread started");
  VideoState * videoState = (VideoState *) arg;

  //usleep(1000); //(100000);
  LOG("Audio sample rate %d",videoState->audio_ctx->sample_rate);

  // ------------ ALSA init --------------
  /* Open PCM device for playback. */
  rc = snd_pcm_open(&handle, "default",
                    SND_PCM_STREAM_PLAYBACK, 0);
  if (rc < 0) {
    LOG_E("unable to open pcm device: %s", snd_strerror(rc));
    videoState->quit = 1;
    return NULL;
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(handle, params, videoState->audio_ctx->channels);

  /* 44100 bits/second sampling rate (CD quality) */
  val = videoState->audio_ctx->sample_rate;
  snd_pcm_hw_params_set_rate_near(handle, params,
                                  &val, &dir);

  /* Set period size to 32 frames. */
  frames = 32;
  snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

  /* Write the parameters to the driver */
  rc = snd_pcm_hw_params(handle, params);
  if (rc < 0) {
    LOG_E("unable to set hw parameters: %s\n", snd_strerror(rc));
    return NULL;
  }

  /* Use a buffer large enough to hold one period */
  snd_pcm_hw_params_get_period_size(params, &frames, &dir);
  size = frames * 4; /* 2 bytes/sample, 2 channels */
  buffer = (char *) malloc(size);

  LOG("size = %d, frames = %d", size, frames);

  /* We want to loop for 5 seconds */
  snd_pcm_hw_params_get_period_time(params, &val, &dir);

  while (videoState->quit == 0) {
    //loops--;
    audio_callback(arg, buffer, size);
    // rc = read(0, buffer, size);
    // if (rc == 0) {
    //   fprintf(stderr, "end of file on input\n");
    //   break;
    // } else if (rc != size) {
    //   fprintf(stderr,
    //           "short read: read %d bytes\n", rc);
    // }
    rc = snd_pcm_writei(handle, buffer, frames);
    if (rc == -EPIPE) {
      /* EPIPE means underrun */
      fprintf(stderr, "underrun occurred\n");
      snd_pcm_prepare(handle);
    } else if (rc < 0) {
      fprintf(stderr,
              "error from writei: %s\n",
              snd_strerror(rc));
    }  else if (rc != (int)frames) {
      fprintf(stderr,
              "short write, write %d frames\n", rc);
    }
  }


  // while (videoState->quit == 0) {
  //   audio_callback(arg, buf, sizeof(buf));
  //   //usleep(50000);
  //   nanosleep(&tm, NULL);
  // }

  snd_pcm_drain(handle);
  snd_pcm_close(handle);
  free(buffer);

  LOG("exit audio thread");
}

/**
 * Pull in data from audio_decode_frame(), store the result in an intermediary
 * buffer, attempt to write as many bytes as the amount defined by len to
 * stream, and get more data if we don't have enough yet, or save it for later
 * if we have some left over.
 *
 * @param   userdata    the pointer we gave to SDL.
 * @param   stream      the buffer we will be writing audio data to.
 * @param   len         the size of that buffer.
 */
void audio_callback(void * userdata, Uint8 * stream, int len)
{
  // retrieve the VideoState
  VideoState * videoState = (VideoState *)userdata;

  double pts;

  // while the length of the audio data buffer is > 0
  while (len > 0) {
    // check global quit flag
    if (global_video_state->quit) {
      return;
    }

    // check how much audio is left to writes
    if (videoState->audio_buf_index >= videoState->audio_buf_size) {
      // we have already sent all avaialble data; get more
      int audio_size = audio_decode_frame(
        videoState,
        videoState->audio_buf,
        sizeof(videoState->audio_buf),
        &pts
        );

      // if error
      if (audio_size < 0) {
        printf("audio error\n");
        // output silence
        videoState->audio_buf_size = 1024;

        // clear memory
        memset(videoState->audio_buf, 0, videoState->audio_buf_size);

        LOG("audio_decode_frame() failed.\n");
      } else {
        audio_size = synchronize_audio(videoState, (int16_t *)videoState->audio_buf, audio_size);

        // cast to usigned just to get rid of annoying warning messages
        videoState->audio_buf_size = (unsigned)audio_size;
      }

      videoState->audio_buf_index = 0;
    }

    int len1 = videoState->audio_buf_size - videoState->audio_buf_index;

    if (len1 > len) {
      len1 = len;
    }

    // copy data from audio buffer to the SDL stream
    memcpy(stream, (uint8_t *)videoState->audio_buf + videoState->audio_buf_index, len1);

    len -= len1;
    stream += len1;

    // update global VideoState audio buffer index
    videoState->audio_buf_index += len1;
  }
}

/**
 * Get a packet from the queue if available. Decode the extracted packet. Once
 * we have the frame, resample it and simply copy it to our audio buffer, making
 * sure the data_size is smaller than our audio buffer.
 *
 * @param   aCodecCtx   the audio AVCodecContext used for decoding
 * @param   audio_buf   the audio buffer to write into
 * @param   buf_size    the size of the audio buffer, 1.5 larger than the one
 *                      provided by FFmpeg
 * @param   pts_ptr     a pointer to the pts of the decoded audio frame.
 *
 * @return              0 if everything goes well, -1 in case of error or quit
 */
static int audio_decode_frame(VideoState * videoState, uint8_t * audio_buf, int buf_size, double * pts_ptr)
{
  // allocate AVPacket to read from the audio PacketQueue (audioq)
  AVPacket * avPacket = av_packet_alloc();

  if (avPacket == NULL) {
    LOG("Could not allocate AVPacket.\n");
    return -1;
  }

  // static uint8_t * audio_pkt_data = NULL;
  // static int audio_pkt_size = 0;

  double pts;
  int n;

  // allocate a new frame, used to decode audio packets
  videoState->avFrame = av_frame_alloc();
  if (!videoState->avFrame) {
    LOG("Could not allocate AVFrame.\n");
    return -1;
  }

  int len1 = 0;
  int data_size = 0;

  // infinite loop: read AVPackets from the audio PacketQueue, decode them into
  // audio frames, resample the obtained frame and update the audio buffer
  for (;;) {
    // check global quit flag
    if (videoState->quit) {
      av_frame_free(&videoState->avFrame);
      return -1;
    }

    // check if we obtained an AVPacket from the audio PacketQueue
    while (videoState->audio_pkt_size > 0) {
      int got_frame = 0;

      // get decoded output data from decoder
      int ret = avcodec_receive_frame(videoState->audio_ctx, videoState->avFrame);

      // check and entire audio frame was decoded
      if (ret == 0) {
        got_frame = 1;
      }

      // check the decoder needs more AVPackets to be sent
      if (ret == AVERROR(EAGAIN)) {
        ret = 0;
      }

      if (ret == 0) {
        // give the decoder raw compressed data in an AVPacket
        ret = avcodec_send_packet(videoState->audio_ctx, avPacket);
      }

      // check the decoder needs more AVPackets to be sent
      if (ret == AVERROR(EAGAIN)) {
        ret = 0;
      } else if (ret < 0) {
        LOG("avcodec_receive_frame decoding error.\n");
        av_frame_free(&videoState->avFrame);
        return -1;
      } else {
        len1 = avPacket->size;
      }

      if (len1 < 0) {
        // if error, skip frame
        videoState->audio_pkt_size = 0;
        break;
      }

      videoState->audio_pkt_data += len1;
      videoState->audio_pkt_size -= len1;
      data_size = 0;

      // if we decoded an entire audio frame
      if (got_frame) {
        // apply audio resampling to the decoded frame
        data_size = audio_resampling(
          videoState,
          videoState->avFrame,
          AV_SAMPLE_FMT_S16,
          audio_buf
          );

        assert(data_size <= buf_size);
      }

      if (data_size <= 0) {
        // no data yet, get more frames
        continue;
      }

      // keep audio_clock up-to-date
      pts = videoState->audio_clock;
      *pts_ptr = pts;
      n = 2 * videoState->audio_ctx->channels;
      videoState->audio_clock += (double)data_size / (double)(n * videoState->audio_ctx->sample_rate);

      if (avPacket->data) {
        // wipe the packet
        av_packet_unref(avPacket);
      }
      av_frame_free(&videoState->avFrame);

      // we have the data, return it and come back for more later
      return data_size;
    }

    if (avPacket->data) {
      // wipe the packet
      av_packet_unref(avPacket);
    }

    // get more audio AVPacket
    int ret = packet_queue_get(&videoState->audioq, avPacket, 1);

    // if packet_queue_get returns < 0, the global quit flag was set
    if (ret < 0) {
      return -1;
    }

    if (avPacket->data == videoState->flush_pkt.data) {
      avcodec_flush_buffers(videoState->audio_ctx);

      continue;
    }

    videoState->audio_pkt_data = avPacket->data;
    videoState->audio_pkt_size = avPacket->size;

    // keep audio_clock up-to-date
    if (avPacket->pts != AV_NOPTS_VALUE) {
      videoState->audio_clock = av_q2d(videoState->audio_st->time_base) * avPacket->pts;
    }
  }

  av_frame_free(&videoState->avFrame);
  return 0;
}

/**
 * Resamples the audio data retrieved using FFmpeg before playing it.
 *
 * @param   videoState          the global VideoState reference.
 * @param   decoded_audio_frame the decoded audio frame.
 * @param   out_sample_fmt      audio output sample format (e.g. AV_SAMPLE_FMT_S16).
 * @param   out_buf             audio output buffer.
 *
 * @return                      the size of the resampled audio data.
 */
static int audio_resampling(VideoState * videoState, AVFrame * decoded_audio_frame, enum AVSampleFormat out_sample_fmt, uint8_t * out_buf)
{
  // get an instance of the AudioResamplingState struct
  AudioResamplingState * arState = getAudioResampling(videoState->audio_ctx->channel_layout);

  if (!arState->swr_ctx) {
    LOG("swr_alloc error.\n");
    return -1;
  }

  // get input audio channels
  arState->in_channel_layout = (videoState->audio_ctx->channels ==
                                av_get_channel_layout_nb_channels(videoState->audio_ctx->channel_layout)) ?
    videoState->audio_ctx->channel_layout :
    av_get_default_channel_layout(videoState->audio_ctx->channels);

  // check input audio channels correctly retrieved
  if (arState->in_channel_layout <= 0) {
    LOG("in_channel_layout error.\n");
    return -1;
  }

  // set output audio channels based on the input audio channels
  if (videoState->audio_ctx->channels == 1) {
    arState->out_channel_layout = AV_CH_LAYOUT_MONO;
  } else if (videoState->audio_ctx->channels == 2) {
    arState->out_channel_layout = AV_CH_LAYOUT_STEREO;
  } else {
    arState->out_channel_layout = AV_CH_LAYOUT_SURROUND;
  }

  // retrieve number of audio samples (per channel)
  arState->in_nb_samples = decoded_audio_frame->nb_samples;
  if (arState->in_nb_samples <= 0) {
    LOG("in_nb_samples error.\n");
    return -1;
  }

  // Set SwrContext parameters for resampling
  av_opt_set_int(
    arState->swr_ctx,
    "in_channel_layout",
    arState->in_channel_layout,
    0
    );

  // Set SwrContext parameters for resampling
  av_opt_set_int(
    arState->swr_ctx,
    "in_sample_rate",
    videoState->audio_ctx->sample_rate,
    0
    );

  // Set SwrContext parameters for resampling
  av_opt_set_sample_fmt(
    arState->swr_ctx,
    "in_sample_fmt",
    videoState->audio_ctx->sample_fmt,
    0
    );

  // Set SwrContext parameters for resampling
  av_opt_set_int(
    arState->swr_ctx,
    "out_channel_layout",
    arState->out_channel_layout,
    0
    );

  // Set SwrContext parameters for resampling
  av_opt_set_int(
    arState->swr_ctx,
    "out_sample_rate",
    videoState->audio_ctx->sample_rate,
    0
    );

  // Set SwrContext parameters for resampling
  av_opt_set_sample_fmt(
    arState->swr_ctx,
    "out_sample_fmt",
    out_sample_fmt,
    0
    );

  // initialize SWR context after user parameters have been set
  int ret = swr_init(arState->swr_ctx);;
  if (ret < 0) {
    LOG("Failed to initialize the resampling context.\n");
    return -1;
  }

  arState->max_out_nb_samples = arState->out_nb_samples = av_rescale_rnd(
    arState->in_nb_samples,
    videoState->audio_ctx->sample_rate,
    videoState->audio_ctx->sample_rate,
    AV_ROUND_UP
    );

  // check rescaling was successful
  if (arState->max_out_nb_samples <= 0) {
    LOG("av_rescale_rnd error.\n");
    return -1;
  }

  // get number of output audio channels
  arState->out_nb_channels = av_get_channel_layout_nb_channels(arState->out_channel_layout);

  // allocate data pointers array for arState->resampled_data and fill data
  // pointers and linesize accordingly
  ret = av_samples_alloc_array_and_samples(
    &arState->resampled_data,
    &arState->out_linesize,
    arState->out_nb_channels,
    arState->out_nb_samples,
    out_sample_fmt,
    0
    );

  // check memory allocation for the resampled data was successful
  if (ret < 0) {
    LOG("av_samples_alloc_array_and_samples() error: Could not allocate destination samples.\n");
    return -1;
  }

  // retrieve output samples number taking into account the progressive delay
  arState->out_nb_samples = av_rescale_rnd(
    swr_get_delay(arState->swr_ctx, videoState->audio_ctx->sample_rate) + arState->in_nb_samples,
    videoState->audio_ctx->sample_rate,
    videoState->audio_ctx->sample_rate,
    AV_ROUND_UP
    );

  // check output samples number was correctly rescaled
  if (arState->out_nb_samples <= 0) {
    LOG("av_rescale_rnd error\n");
    return -1;
  }

  if (arState->out_nb_samples > arState->max_out_nb_samples) {
    // free memory block and set pointer to NULL
    av_free(arState->resampled_data[0]);

    // Allocate a samples buffer for out_nb_samples samples
    ret = av_samples_alloc(
      arState->resampled_data,
      &arState->out_linesize,
      arState->out_nb_channels,
      arState->out_nb_samples,
      out_sample_fmt,
      1
      );

    // check samples buffer correctly allocated
    if (ret < 0) {
      LOG("av_samples_alloc failed.\n");
      return -1;
    }

    arState->max_out_nb_samples = arState->out_nb_samples;
  }

  if (arState->swr_ctx) {
    // do the actual audio data resampling
    ret = swr_convert(
      arState->swr_ctx,
      arState->resampled_data,
      arState->out_nb_samples,
      (const uint8_t **)decoded_audio_frame->data,
      decoded_audio_frame->nb_samples
      );

    // check audio conversion was successful
    if (ret < 0) {
      LOG("swr_convert_error.\n");
      return -1;
    }

    // get the required buffer size for the given audio parameters
    arState->resampled_data_size = av_samples_get_buffer_size(
      &arState->out_linesize,
      arState->out_nb_channels,
      ret,
      out_sample_fmt,
      1
      );

    // check audio buffer size
    if (arState->resampled_data_size < 0) {
      LOG("av_samples_get_buffer_size error.\n");
      return -1;
    }
  } else {
    LOG("swr_ctx null error.\n");
    return -1;
  }

  // copy the resampled data to the output buffer
  memcpy(out_buf, arState->resampled_data[0], arState->resampled_data_size);

  /*
   * Memory Cleanup.
   */
  if (arState->resampled_data) {
    // free memory block and set pointer to NULL
    av_freep(&arState->resampled_data[0]);
  }

  av_freep(&arState->resampled_data);
  arState->resampled_data = NULL;

  if (arState->swr_ctx) {
    // free the allocated SwrContext and set the pointer to NULL
    swr_free(&arState->swr_ctx);
  }

  return arState->resampled_data_size;
}

/**
 * Initializes an instance of the AudioResamplingState Struct with the given
 * parameters.
 *
 * @param   channel_layout  the audio codec context channel layout to be used.
 *
 * @return                  the allocated and initialized AudioResamplingState
 *                          struct instance.
 */
static AudioResamplingState * getAudioResampling(uint64_t channel_layout)
{
  AudioResamplingState * audioResampling = av_mallocz(sizeof(AudioResamplingState));

  audioResampling->swr_ctx = swr_alloc();
  audioResampling->in_channel_layout = channel_layout;
  audioResampling->out_channel_layout = AV_CH_LAYOUT_STEREO;
  audioResampling->out_nb_channels = 0;
  audioResampling->out_linesize = 0;
  audioResampling->in_nb_samples = 0;
  audioResampling->out_nb_samples = 0;
  audioResampling->max_out_nb_samples = 0;
  audioResampling->resampled_data = NULL;
  audioResampling->resampled_data_size = 0;

  return audioResampling;
}

/**
 *
 * @param videoState
 * @param pos
 * @param rel
 */
static void stream_seek(VideoState * videoState, int64_t pos, int rel)
{
  pthread_mutex_lock(&videoState->screen_mutex);
  if (!videoState->seek_req) {
    videoState->seek_pos = pos;
    videoState->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
    videoState->seek_req = 1;
  }
  pthread_mutex_unlock(&videoState->screen_mutex);
}
