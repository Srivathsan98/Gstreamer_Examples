#include <iostream>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <string>

using namespace std;
using namespace cv;

int counter = 0;
bool isRecording = false;
GstElement *pipeline;
GstElement *source;

void create_pipeline()
{
    GstElement *videoconvert, *encoder, *mux, *sink;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    GstCaps *caps;
    string file_path = "video" + to_string(counter) + ".mp4";

    pipeline = gst_pipeline_new("videorecord-pipeline");
    source = gst_element_factory_make("appsrc", "appsrc");
    videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    encoder = gst_element_factory_make("x264enc", "encoder");
    mux = gst_element_factory_make("qtmux", "mux");
    sink = gst_element_factory_make("filesink", "filesink");

    if(!pipeline || !source || !videoconvert || !encoder || !mux || !sink)
    {
        cerr << "not all elements could be created" << endl;
        return;
    }

    caps = gst_caps_new_simple("video/x-raw", 
                                "format", G_TYPE_STRING, "BGR", 
                                "width", G_TYPE_INT, 1280, 
                                "height", G_TYPE_INT, 720,
                                "framerate", GST_TYPE_FRACTION, 30, 1,
                                NULL);
    
    // g_object_set(source, "caps", caps, "format", GST_FORMAT_TIME, NULL);
    g_object_set(source, "caps", caps, NULL);
    g_object_set(source, "format", GST_FORMAT_TIME, "is-live", TRUE, "do-timestamp", TRUE, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(pipeline), source, videoconvert, encoder, mux, sink, NULL);

    if(!gst_element_link_many(source, videoconvert, encoder, mux, sink, NULL))
    {
        cerr << "could not link elements" << endl;
        gst_object_unref(pipeline);
        return;
    }

    g_object_set(sink, "location", file_path.c_str(), NULL);
    // g_object_set(encoder, "bitrate", 5000, "speed-preset", "ultrafast", NULL);
    g_object_set(G_OBJECT(encoder), "bitrate", 5000, NULL);
    g_object_set(G_OBJECT(encoder), "speed-preset", 1, NULL);



    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if(ret == GST_STATE_CHANGE_FAILURE)
    {
        cerr << "unable to set the pipeline to the playing state" << endl;
        gst_object_unref(pipeline);
        return;
    }

    isRecording = true;
    cout << "Recording started: " << file_path << endl;
}

void stoppipeline()
{
    // if(!pipeline)
    // {
    //     cerr << "pipeline not created" << endl;
    //     return;
    // }
    // if(isRecording)
    // {
    //     gst_element_send_event(pipeline, gst_event_new_eos());

    //     GstBus *bus = gst_element_get_bus(pipeline);
    //     GstMessage *msg;
    //     // do {
    //     msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
    //                                      (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
    //     if (msg)
    //     {
    //         gst_message_unref(msg);
    //         gst_app_src_end_of_stream(source);
    //     }
    //     // } while (msg);
    //     gst_object_unref(bus);
    //     gst_element_set_state(pipeline, GST_STATE_NULL);
    //     gst_object_unref(pipeline);
    //     pipeline = nullptr;
    //     counter++;
    //     // file_path = "video" + to_string(counter) + ".mp4";
    //     cout << "Recording stopped" << endl;
    //     isRecording = false;

    // }
    // if (!pipeline)
    // {
    //     cerr << "Pipeline not created" << endl;
    //     return;
    // }

    // if (isRecording)
    // {
    //     cout << "Sending EOS..." << endl;

    //     // Send EOS event to pipeline
    //     gst_element_send_event(pipeline, gst_event_new_eos());

    //     // Wait for EOS to be received
    //     GstBus *bus = gst_element_get_bus(pipeline);
    //     GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
    //                                                  (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

    //     if (msg != NULL)
    //     {
    //         if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
    //         {
    //             cerr << "Error occurred while stopping pipeline!" << endl;
    //         }
    //         gst_message_unref(msg);
    //     }

    //     gst_object_unref(bus);

    //     // Stop the pipeline properly
    //     gst_element_set_state(pipeline, GST_STATE_NULL);
    //     gst_object_unref(pipeline);
    //     pipeline = nullptr;

    //     cout << "Recording stopped and finalized." << endl;
    //     isRecording = false;
    //     counter++;
    // }
    if (!pipeline)
    {
        cerr << "Pipeline not created" << endl;
        return;
    }

    if (isRecording)
    {
        cout << "Sending EOS..." << endl;

        // Send EOS event
        gst_element_send_event(pipeline, gst_event_new_eos());

        // Wait for EOS message
        GstBus *bus = gst_element_get_bus(pipeline);
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
                                                     (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

        if (msg != NULL)
        {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
            {
                GError *err = NULL;
                gchar *debug_info = NULL;
                gst_message_parse_error(msg, &err, &debug_info);
                cerr << "Error received: " << err->message << endl;
                if (debug_info)
                    cerr << "Debug info: " << debug_info << endl;
                g_clear_error(&err);
                g_free(debug_info);
            }
            gst_message_unref(msg);
        }
        else
        {
            cerr << "No EOS message received! Possible pipeline issue." << endl;
        }

        gst_object_unref(bus);

        // Stop the pipeline properly
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;

        cout << "Recording stopped and finalized." << endl;
        isRecording = false;
        counter++;
    }
}

void sendFrame(cv::Mat frame)
{
    if(!isRecording)
    {
        cerr << "not recording" << endl;
        return;
    }
    GstBuffer *buffer;
    GstFlowReturn ret;
    GstMapInfo map;
    guint size;

    // cv::cvtColor(frame, frame, cv::COLOR_BGR2RGBA);
    size = frame.total() * frame.elemSize();
    buffer = gst_buffer_new_allocate(NULL, size, NULL);
    if(gst_buffer_map(buffer, &map, GST_MAP_WRITE))
    {
        memcpy(map.data, frame.data, size);
        gst_buffer_unmap(buffer, &map);
    }
    else
    {
        cerr << "failed to map the buffer" << endl;
        gst_buffer_unref(buffer);
        return;
    }

   // ✅ Retrieve pipeline clock time
    GstClockTime timestamp = gst_element_get_clock(pipeline) ? 
                             gst_clock_get_time(gst_element_get_clock(pipeline)) : 0;
    GstClockTime running_time = timestamp - gst_element_get_base_time(pipeline);

    // ✅ Set buffer timestamps based on pipeline running time
    GST_BUFFER_PTS(buffer) = running_time;
    GST_BUFFER_DTS(buffer) = running_time;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale(1, GST_SECOND, 30); // Assuming 30 FPS

    // ✅ Mark buffer as keyframe
    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_LIVE);

    g_signal_emit_by_name(source, "push-buffer", buffer, &ret);
    gst_buffer_unref(buffer);

    if(ret != GST_FLOW_OK)
    {
        cerr << "error pushing buffer" << endl;
        stoppipeline();
        return;
    }


}


int main()
{
    gst_init(NULL, NULL);
    // create_pipeline();
    std::string pipeline = "v4l2src device=/dev/video0 ! videoconvert ! appsink";
    cv::VideoCapture cap(pipeline, cv::CAP_GSTREAMER);

    if(!cap.isOpened())
    {
        return -1;
    }

    cv::Mat frame;

    while(true)
    {
        if(!cap.read(frame))
        {
            std::cerr << "error cant read frames" << std::endl;
            break;
        }

        cv::imshow("feed", frame);
        cout << "frame col:" << frame.cols << "rows" << frame.rows << endl;
        char key = (char)cv::waitKey(1);

        if(key == 'q')
        {
            break;
        }

        else if(key == 'r' && !isRecording)
        {
            cout << "creating pipeline" << endl;
            create_pipeline();
        }
        
        else if(key == 's' && isRecording)
        {
            cout << "stopping" << endl;
            stoppipeline();
        }

        if(isRecording)
        {
            sendFrame(frame);
        }
    }
    if(isRecording)
    {
        stoppipeline();
    }
    return 0;
}