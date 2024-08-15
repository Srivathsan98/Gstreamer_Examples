// /*extract frames from a video using gstreamer*/

// /*command - gst-launch-1.0 filesrc location=sample_720p.mp4 ! decodebin ! videoconvert ! jpegenc ! multifilesink location=%05d.jpg*/

// /************************************************************************************************************* */

#include <iostream>
#include <gstreamer-1.0/gst/gst.h>


/*Function for performing bus watch with incoming messages in the bus. used as a callback function for the gst_bus_watch_id
it keeps checking the bus in a loop and once it receives ERROR/EOS message it exits.*/
static gboolean 
bus_call (GstBus *bus,
            GstMessage *msg,
            gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;
    GstElement *pipeline;

    switch (GST_MESSAGE_TYPE (msg))
    {
    case GST_MESSAGE_EOS:
        g_print ("End of Stream\n");
        g_main_loop_quit(loop);
        break;
    case GST_MESSAGE_ERROR:
    {
        gchar* debug;
        GError *error;
        gst_message_parse_error (msg, &error, &debug);
        g_free(debug);

        g_printerr ("Error: %s\n", error->message);
        g_error_free(error);
    }

    case GST_MESSAGE_BUFFERING:
    {
        gint percent=0;
        gst_message_parse_buffering(msg, &percent);
        g_print("Buffering (%3d%%)\n", percent);
        if(percent < 100)
        {
            gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PAUSED);
        }
        else
        {
            gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
        }
        break;
    }
    
    default:
        break;
    }

    return TRUE;
}

/*used to create a pad for decoder and link it with the next element in the pipeline. we are using this as we are using general decode element - decodebin.
it uses a decoder based in the analyzed frame format.*/
static void
on_pad_added (GstElement *element,
                GstPad *pad,
                gpointer data)
{
    GstPad *sinkpad;
    GstElement *conv = (GstElement *) data;
    g_print("Dynamic pad created, linking this with decoder\n");

    sinkpad = gst_element_get_static_pad (conv, "sink");

    if (gst_pad_is_linked(sinkpad)) {
        g_object_unref(sinkpad);
        return;
    }

    if(gst_pad_link (pad, sinkpad) != GST_PAD_LINK_OK)
    {
        g_printerr("not pad llinked\n");
    }
    gst_object_unref(sinkpad);
}

int main(int argc, char *argv[])
{
    GMainLoop *loop;

    GstElement *pipeline, *source, *decoder, *vidrate, *conv, *encoder, *sink;
    GstBus *bus;
    guint bus_watch_id;

    gst_init(&argc, &argv);

    loop = g_main_loop_new (NULL, FALSE);

    if(argc != 2)
    {
        g_printerr ("Usage %s \n", argv[0]);
        return -1;
    }

    pipeline = gst_pipeline_new ("frame_extract");
    source = gst_element_factory_make ("filesrc",   "file-source");
    decoder = gst_element_factory_make ("decodebin", "decoder-bin");
    vidrate = gst_element_factory_make ("videorate", "videorater");
    conv = gst_element_factory_make ("videoconvert", "videoconverter");
    encoder = gst_element_factory_make ("jpegenc", "jpeg-encoder");
    sink = gst_element_factory_make ("multifilesink", "multiple files");

    if (!pipeline || ! source || !decoder || !vidrate || !conv || !encoder || !sink)
    {
        g_printerr ("could not create element\n");
        return -1;
    }

    g_object_set (G_OBJECT (source), "location", argv[1], NULL);
    g_object_set (G_OBJECT (encoder), "quality", 85, NULL);
    // g_object_set (G_OBJECT (source), "location", "sample_720.mp4", NULL);
    g_object_set (G_OBJECT (sink), "location", "frame%05d.jpg", "sync", FALSE, NULL);

    

    gst_bin_add_many (GST_BIN (pipeline),
                        source, decoder, vidrate, conv, encoder, sink, NULL);
    
    gst_element_link (source, decoder);
    gst_element_link_many (vidrate, conv, encoder, sink, NULL);
    g_signal_connect (decoder, "pad-added", G_CALLBACK (on_pad_added), vidrate);

    bus = gst_pipeline_get_bus(GST_PIPELINE (pipeline));
    bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    g_print("NOW_PLAYING\n");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    g_print("RUNNING\n");
    g_main_loop_run (loop);

    

    g_print("Returned\n");
    gst_element_set_state(pipeline, GST_STATE_NULL);
    
    g_print("DELETED\n");
    gst_object_unref(GST_OBJECT (pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);

    return 0;
}

/*when running the program if the folder where the frames are stored are open the we may get few corrupt/damaged/garbage frames but when we close the folder and open then it works fine.*/

