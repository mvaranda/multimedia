/******************************************
 * 
 * Simple messaging system across threads
 * 
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 * 
 ******************************************/
#include "log.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t log_mutex;

#define LOCK() pthread_mutex_lock(&log_mutex)
#define UNLOCK() pthread_mutex_unlock(&log_mutex)

#define BUF_SIZE 256

void log_init(void)
{
  if (pthread_mutex_init(&log_mutex, NULL) != 0) {
    printf("log_init: mutex failed\n");
  }
}

void log_line(char * what, int line, char * fmt, ...)
{
  char buffer[BUF_SIZE];

  va_list ap;
  va_start(ap, fmt); /* Initialize the va_list */

  vsnprintf (buffer, sizeof(buffer), fmt, ap);
  va_end(ap); /* Cleanup the va_list */

  LOCK();
  printf("%s (%d): %s\n", what, line, buffer);
  UNLOCK();

}

