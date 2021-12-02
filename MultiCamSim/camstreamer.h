

#pragma once

#include "log.h"
#include "gst/gst.h"
#include <QThread>

class CamStreamer: public QThread {
public:
    CamStreamer();
    ~CamStreamer();

    GstBus *getBus();

private:
    void run();
    GstElement *pipeline;
    GstBus *bus;

};
