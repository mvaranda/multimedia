
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

#define VIDEO_SAMPLE_URI "/home/mvaranda/Videos/sintel_trailer-480p.webm"
#define PIPELINE_PLAYBIN "playbin uri=file://"

//#define VIDEO_SAMPLE "playbin uri=https://www.freedesktop.org/software/gstreamer-sdk/data/media/sintel_trailer-480p.webm"
#define VIDEO_SAMPLE "playbin uri=file:///home/mvaranda/Videos/sintel_trailer-480p.webm"

//#define VIDEO_SAMPLE  "v4l2src ! textoverlay text=\"Cam 1\" valignment=top halignment=left font-desc=\"Sans, 14\" ! autovideosink"
//#define VIDEO_SAMPLE  "v4l2src ! textoverlay text=\"Cam 1\" valignment=top halignment=left font-desc=\"Sans, 14\" ! playsink"
CamStreamer *cam_streamer;

static void realize_cb (void *widget, GstElement *sink)
{
//  GdkWindow *window = gtk_widget_get_window (widget);
  guintptr window_handle = cam_streamer->win_id;
  LOG("************************** CB ***************************");

//  if (!gdk_window_ensure_native (window))
//    g_error ("Couldn't create native window needed for GstXOverlay!");

//#if defined (GDK_WINDOWING_WIN32)
//    window_handle = (guintptr)GDK_WINDOW_HWND (window);
//#elif defined (GDK_WINDOWING_QUARTZ)
//      window_handle = gdk_quartz_window_get_nsview (window);
//#elif defined (GDK_WINDOWING_X11)
//        window_handle = GDK_WINDOW_XID (window);
//#endif

  gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (sink), window_handle);
}


CamStreamer::~CamStreamer()
{
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
}

CamStreamer::CamStreamer(WId id)
{
    cam_streamer = this;
    win_id = id;
    /* Initialize GStreamer */
    gst_init (0, 0);

    /* Build the pipeline */
    pipeline = gst_parse_launch (PIPELINE_PLAYBIN VIDEO_SAMPLE_URI, NULL);
    //pipeline = gst_parse_launch ("uridecodebin uri=file:///home/mvaranda/Videos/sintel_trailer-480p.webm name=d ! xvimagesink name=sink d. ! pulsesink", NULL);

#if 0
    GtkWidget *da = gtk_drawing_area_new();
    GstElement * sink = gst_bin_get_by_name (GST_BIN (pipeline), "sink");
    g_signal_connect (da, "realize", G_CALLBACK (realize_cb), sink);
#endif

#if 1
    // ref: https://stackoverflow.com/questions/12902574/using-textoverlay-and-timeoverlay-together
    // Set sample text in textoverlay
    //GstElement *videosink = gst_element_factory_make ("sdlvideosink", "videosink"); // ximagesink
    GstElement *videosink = gst_element_factory_make ("ximagesinkxx", "video_xxsink"); // ximagesink

    GstElement *textoverlay = gst_element_factory_make("textoverlay", "textoverlay");
    GstElement *timeoverlay = gst_element_factory_make("timeoverlay", "timeoverlay");

    if (!videosink || !textoverlay || !timeoverlay) {
        LOG_E("error: videosink = %p, textoverlay = %p, timeoverlay = %p", videosink, textoverlay, timeoverlay);
        return;
    }

    g_object_set(G_OBJECT(textoverlay), "text", "Test Subtitle", NULL);

    // Create bin, add elements
    GstElement *mybin = gst_bin_new("mybin");
    gst_bin_add (GST_BIN (mybin), videosink);
    gst_bin_add (GST_BIN (mybin), textoverlay);
    gst_bin_add (GST_BIN (mybin), timeoverlay);

    // Get sink pad for textoverlay and make it a ghostpad for bin
    //GstPad *pad_textoverlay_sink = gst_element_get_pad(textoverlay, "video_sink");
    GstPad *pad_textoverlay_sink = gst_element_get_static_pad(textoverlay, "video_sink");
    if ( ! pad_textoverlay_sink) {
        LOG_E("no pad_textoverlay_sink");
    }

    gst_element_add_pad(mybin, gst_ghost_pad_new("sink", (GstPad*) pad_textoverlay_sink));

    // Link elements: textoverlay -> timeoverlay -> videosink
    gst_element_link_pads(textoverlay, "src", timeoverlay, "sink");
    gst_element_link_pads(timeoverlay, "src", videosink, "sink");

    // Conect the bin to the playbin
    g_object_set (G_OBJECT (pipeline), "video-sink", mybin, NULL);
//    GstElement *pipeline = gst_element_factory_make ("playbin2", "playbin2");
//    g_object_set (G_OBJECT (pipeline), "uri", VIDEO_SAMPLE_URI, NULL);
#endif
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
