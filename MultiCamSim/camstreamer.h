
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

#pragma once

#include "log.h"
#include "gst/gst.h"
#include <QThread>
#include <QWidget>

class CamStreamer: public QThread {
public:
    CamStreamer() {};
    CamStreamer(WId id);
    ~CamStreamer();

    GstBus *getBus();
    WId win_id;

private:
    void run();
    GstElement *pipeline;
    GstBus *bus;

};
