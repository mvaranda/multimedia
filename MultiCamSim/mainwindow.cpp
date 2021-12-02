#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QLabel>

//-------------- gstreamer --------------
#include "gst/gst.h"

//#define VIDEO_SAMPLE "playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm"
#define VIDEO_SAMPLE "playbin uri=file:///home/mvaranda/Videos/sintel_trailer-480p.webm"

void player_thread(void)
{
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;

    /* Initialize GStreamer */
    gst_init (0, 0);

    /* Build the pipeline */
    pipeline =
        gst_parse_launch
        (VIDEO_SAMPLE,
        NULL);

    /* Start playing */
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
    bus = gst_element_get_bus (pipeline);
    msg =
        gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, (GstMessageType)
        (GST_MESSAGE_ERROR | GST_MESSAGE_EOS)); //  (GstBus * bus, GstClockTime timeout, GstMessageType types);


    /* See next tutorial for proper error message handling/parsing */
    if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
      g_error ("An error occurred! Re-run with the GST_DEBUG=*:WARN environment "
          "variable set for more details.");
    }

    /* Free resources */
    gst_message_unref (msg);
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
}
//---------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , gst_thread(nullptr)
{
    ui->setupUi(this);
    video_area_label = ui->video_area;
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_bt_play_clicked()
{
    if (gst_thread == nullptr) {
        gst_thread = QThread::create(player_thread);
        gst_thread->start();
        video_area_label->setText(tr("Started player"));
    }
    else {
        video_area_label->setText(tr("Player already running"));
    }
}

