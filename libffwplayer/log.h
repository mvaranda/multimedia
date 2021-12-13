
/******************************************
 * 
 * Simple messaging system across threads
 * 
 * License: GPL-3
 * Copyrights: Marcelo Varanda
 * 
 ******************************************/

#pragma once

/**
 * @file
 * @brief logger
 * 
 *
 */

// for debugging:
#define L() printf("line: %d, %s, %s\n", __LINE__, __FILE__, __func__) /**< a short macro to assist debugging. will be removed soon */

#define UNUSED(x) (void)(x) /**<  this macro can be used to avoid warning/error of unused variable */

#define LOG(fmt,...)   log_line((char *) "DEB",  __LINE__, (char *) fmt, ##__VA_ARGS__) /**< debug: its format is like printf */
#define LOG_I(fmt,...)   log_line((char *) "INF",  __LINE__, (char *) fmt, ##__VA_ARGS__) /**< debug: its format is like printf */
#define LOG_W(fmt,...)   log_line((char *) "WRN",  __LINE__, (char *) fmt, ##__VA_ARGS__) /**< debug: its format is like printf */
#define LOG_E(fmt,...)   log_line((char *) "ERR",  __LINE__, (char *) fmt, ##__VA_ARGS__) /**< debug: its format is like printf */


#ifdef __cplusplus
  extern "C" {
#endif

/**
 * @brief internal function.
 * @warning App should NOT call this function 
 */
void log_init(void);

/**
 * @brief Internal function. 
 * @warning App should NOT call this function
 * 
 */
void log_line(char * what, int line, char * fmt, ...);



#ifdef __cplusplus
  }
#endif
