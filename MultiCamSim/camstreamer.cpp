
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

#include "camstreamer.h"
#include "gst/video/videooverlay.h"

//#define VIDEO_SAMPLE "playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm"
#define VIDEO_SAMPLE "playbin uri=file:///home/mvaranda/Videos/sintel_trailer-480p.webm"


CamStreamer::~CamStreamer()
{
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
}

CamStreamer::CamStreamer(WId id)
{
    win_id = id;
    /* Initialize GStreamer */
    gst_init (0, 0);

    /* Build the pipeline */
    pipeline = gst_parse_launch (VIDEO_SAMPLE, NULL);

    /* Start playing */
    // gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

void CamStreamer::run()
{
    GstMessage *msg;

    guintptr window_handle = win_id; //GDK_WINDOW_XID (win_id);

//    if (!gdk_window_ensure_native (win_id))
//      g_error ("Couldn't create native window needed for GstVideoOverlay!");

    /* Pass it to playbin, which implements VideoOverlay and will forward it to the video sink */
    gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (pipeline), window_handle);

    /* Start playing */
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* Wait until error or EOS */
    bus = gst_element_get_bus (pipeline);
    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, (GstMessageType)
        (GST_MESSAGE_ERROR | GST_MESSAGE_EOS)); //  (GstBus * bus, GstClockTime timeout, GstMessageType types);


    /* See next tutorial for proper error message handling/parsing */
    if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
      g_error ("An error occurred! Re-run with the GST_DEBUG=*:WARN environment "
          "variable set for more details.");
    }

    /* Free resources */
    gst_message_unref (msg);
}
