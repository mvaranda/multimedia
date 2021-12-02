
/**********************************************************
 *
 *        M u l t i   C a m e r a   S i m u l a t o r
 *
 * License: proprietary
 *
 * Copyrights: Marcelo Varanda
 * Initial date: October 2021
 * http://www.varanda.ca
 *
 ***********************************************************/

#include <QDebug>
#include "log.h"

static void qDebugCpp(const char * s) // format,...)
{
//  va_list args;
//  va_start(args, format);
//  qDebug(format, args);
//  va_end(args);
  qDebug("%s", s);
}

extern "C" {
  void qDebugC(const char * format,...)
  {
    char s[1024];
    va_list args;
    va_start(args, format);
    vsprintf (s, format, args );
    qDebugCpp(s); //format, args);
    va_end(args);
  }
}
