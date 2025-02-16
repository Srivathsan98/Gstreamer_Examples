#include <iostream>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <thread>
#include <deque>

using namespace std;
using namespace cv;

std::deque<Mat> buffer;
std::mutex bufferMutex;

#define BUFFER_SIZE 30 * 30
#define FPS 30
#define TIME_SECS 30
#define RECORD_TIME 60

cv::Mat frame;
int counter = 0;
bool isRecording = false;
GstElement *pipeline;
GstElement *source;
std::string timestamp;
int framecounter = 0;
cv::VideoCapture cap;

void sendFrame(cv::Mat recordframe);

void add_timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y_%m_%d_%H:%M:%S");
    timestamp = ss.str();
    std::cout << "[" << timestamp << "] " << "Program started." << std::endl;
}

void capture_frame()
{
    while (true)
    {
        if (!cap.read(frame))
        {
            std::cerr << "error cant read frames" << std::endl;
            return;
        }
        std::lock_guard<std::mutex> lock(bufferMutex);
        buffer.push_back(frame.clone());
        if (buffer.size() > BUFFER_SIZE)
        {
            buffer.pop_front();
        }
        cv::imshow("frame", frame);
        if (cv::waitKey(1) == 'q')
            break;
    }
}

void create_pipeline()
{
    add_timestamp();
    GstElement *videoconvert, *encoder, *mux, *overlay, *text, *sink;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;
    GstCaps *caps;
    string file_path = "video" + timestamp + ".mp4";

    pipeline = gst_pipeline_new("videorecord-pipeline");
    source = gst_element_factory_make("appsrc", "appsrc");
    videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    encoder = gst_element_factory_make("x264enc", "encoder");
    overlay = gst_element_factory_make("timeoverlay", "overlay");
    text = gst_element_factory_make("textoverlay", "text");
    mux = gst_element_factory_make("qtmux", "mux");
    sink = gst_element_factory_make("filesink", "filesink");

    if (!pipeline || !source || !videoconvert || !encoder || !overlay || !text || !mux || !sink)
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

    // gst_bin_add_many(GST_BIN(pipeline), source, videoconvert, encoder, overlay, text, mux, sink, NULL);

    // if (!gst_element_link_many(source, videoconvert, encoder, overlay, text, mux, sink, NULL))
    // {
    //     cerr << "could not link elements" << endl;
    //     gst_object_unref(pipeline);
    //     return;
    // }

    gst_bin_add_many(GST_BIN(pipeline), source, videoconvert, overlay, text, videoconvert, encoder, mux, sink, NULL);

if (!gst_element_link_many(source, videoconvert, overlay, text, videoconvert, encoder, mux, sink, NULL))
{
    cerr << "could not link elements" << endl;
    gst_object_unref(pipeline);
    return;
}

    g_object_set(sink, "location", file_path.c_str(), NULL);
    // g_object_set(encoder, "bitrate", 5000, "speed-preset", "ultrafast", NULL);
    g_object_set(G_OBJECT(encoder), "bitrate", 5000, NULL);
    g_object_set(G_OBJECT(encoder), "speed-preset", 1, NULL);
    g_object_set(G_OBJECT(overlay), "font-desc", "Sans, 48", "valignment", 1, "halignment", 2, NULL);
    g_object_set(G_OBJECT(text), "text", timestamp.c_str(), "valignment", 2, "halignment", 0, NULL);

    ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        cerr << "unable to set the pipeline to the playing state" << endl;
        gst_object_unref(pipeline);
        return;
    }

    isRecording = true;
    cout << "Recording started: " << file_path << endl;
    framecounter = 0;
    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        // buffer.clear();
        for (auto &bufframe : buffer)
        {
            sendFrame(bufframe);
        }
    }
}

void stoppipeline()
{

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
        // GstBus *bus = gst_element_get_bus(pipeline);
        // GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
        //                                              (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

        // if (msg != NULL)
        // {
        //     if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
        //     {
        //         GError *err = NULL;
        //         gchar *debug_info = NULL;
        //         gst_message_parse_error(msg, &err, &debug_info);
        //         cerr << "Error received: " << err->message << endl;
        //         if (debug_info)
        //             cerr << "Debug info: " << debug_info << endl;
        //         g_clear_error(&err);
        //         g_free(debug_info);
        //     }
        //     gst_message_unref(msg);
        // }
        // else
        // {
        //     cerr << "No EOS message received! Possible pipeline issue." << endl;
        // }

        // gst_object_unref(bus);

        // Stop the pipeline properly
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;

        cout << "Recording stopped and finalized." << endl;
        isRecording = false;
        counter++;
    }
}

void sendFrame(cv::Mat recordframe)
{
    if (!isRecording)
    {
        cerr << "not recording" << endl;
        return;
    }
    GstBuffer *recordbuffer;
    GstFlowReturn ret;
    GstMapInfo map;
    guint size;

    // cv::cvtColor(frame, frame, cv::COLOR_BGR2RGBA);
    size = recordframe.total() * recordframe.elemSize();
    recordbuffer = gst_buffer_new_allocate(NULL, size, NULL);
    if (gst_buffer_map(recordbuffer, &map, GST_MAP_WRITE))
    {
        memcpy(map.data, recordframe.data, size);
        gst_buffer_unmap(recordbuffer, &map);
    }
    else
    {
        cerr << "failed to map the buffer" << endl;
        gst_buffer_unref(recordbuffer);
        return;
    }

    // ✅ Retrieve pipeline clock time
    GstClockTime timestamp = gst_element_get_clock(pipeline) ? gst_clock_get_time(gst_element_get_clock(pipeline)) : 0;
    GstClockTime running_time = timestamp - gst_element_get_base_time(pipeline);

    // ✅ Set buffer timestamps based on pipeline running time
    GST_BUFFER_PTS(recordbuffer) = running_time;
    GST_BUFFER_DTS(recordbuffer) = running_time;
    GST_BUFFER_DURATION(recordbuffer) = gst_util_uint64_scale(1, GST_SECOND, 30); // Assuming 30 FPS

    // ✅ Mark buffer as keyframe
    GST_BUFFER_FLAG_SET(recordbuffer, GST_BUFFER_FLAG_LIVE);

    g_signal_emit_by_name(source, "push-buffer", recordbuffer, &ret);
    gst_buffer_unref(recordbuffer);

    if (ret != GST_FLOW_OK)
    {
        cerr << "error pushing buffer" << endl;
        stoppipeline();
        return;
    }

    framecounter++;
    if (framecounter > FPS * RECORD_TIME)
    {
        stoppipeline();
    }
}

int main()
{
    gst_init(NULL, NULL);

    std::string pipeline = "v4l2src device=/dev/video0 ! videoconvert ! appsink";
    cap.open(pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened())
    {
        return -1;
    }

    std::thread capture_thread(capture_frame);

    while (1)
    {
        char key = (char)cv::waitKey(1);

        if (key == 'q')
        {
            exit(0);
        }

        else if (key == 'r' && !isRecording)
        {
            cout << "creating pipeline" << endl;
            create_pipeline();
        }

        // if(isRecording)
        // {
        //     sendFrame(buffer.front());
        // }
    }

    capture_thread.join();
    return 0;
}