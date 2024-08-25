/*Display FPS with live frames
capture the frames using a gstreamer pipeline and then add the FPS to it
and display it using a gstreamer pipeline*/

#include <iostream>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <chrono>

double calcFPS()
{
    static int framecount = 0;
    static double lasttime = (double)cv::getTickCount();
    static double fps = 0;

    framecount++;
    if (framecount >=1 )
    {
        double currenttime = (double)cv::getTickCount();
        fps = framecount / ((currenttime - lasttime) / cv::getTickFrequency());
        lasttime = currenttime;
        framecount = 0;
    }

    return fps;
}

bool capture_and_push_frames(cv::VideoCapture &cap, GstElement *appsrc)
{
    cv::Mat frame;
    while (true)
    {
        if (!cap.read(frame))
        {
            std::cerr << "Error cant read frame" << std::endl;
            return false;
        }

        double fps = calcFPS();
        cv::putText(frame, "FPS:" + std::to_string(fps), cv::Point(30, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 0), 2);

        GstBuffer *buffer = gst_buffer_new_allocate(NULL, frame.total() * frame.elemSize(), NULL);
        if (!buffer)
        {
            std::cerr << "failed to allocate bufer" << std::endl;
            return false;
        }

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE))
        {
            std::cerr << "failed to map the buffer" << std::endl;
            gst_buffer_unref(buffer);
            return false;
        }

        std::memcpy(map.data, frame.data, frame.total() * frame.elemSize());
        gst_buffer_unmap(buffer, &map);

        GstFlowReturn ret;
        g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
        gst_buffer_unref(buffer);

        if (ret != GST_FLOW_OK)
        {
            std::cerr << "error pushing buffer" << std::endl;
            return false;
        }

        if (cv::waitKey(1) == 'q')
        {
            exit(0);
        }
    }
    return true;
}

int main(int argc, char **argv)
{
    gst_init(&argc, &argv);

    GstElement *pipeline = gst_pipeline_new("video-pipeline");
    GstElement *appsrc = gst_element_factory_make("appsrc", "appsrc");
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    GstElement *autovideosink = gst_element_factory_make("autovideosink", "videosink");

    if (!pipeline || !appsrc || !videoconvert || !autovideosink)
    {
        std::cerr << "not all elements could be created" << std::endl;
        return -1;
    }

    GstCaps *caps = gst_caps_new_simple("video/x-raw", 
                                        "format", G_TYPE_STRING, "BGR", 
                                        "width", G_TYPE_INT, 640, 
                                        "height", G_TYPE_INT, 480,
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        NULL);

    g_object_set(appsrc, "caps", caps, "format", GST_FORMAT_TIME, NULL);
    gst_caps_unref(caps);

    gst_bin_add_many(GST_BIN(pipeline), appsrc, videoconvert, autovideosink, NULL);
    if (!gst_element_link_many(appsrc, videoconvert, autovideosink, NULL))
    {
        std::cerr << "could not lnk elements" << std::endl;
        gst_object_unref(pipeline);
        return -1;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    std::string cap_pipeline = "v4l2src device=/dev/video0 ! videoconvert ! appsink";
    cv::VideoCapture cap(cap_pipeline, cv::CAP_GSTREAMER);
    if (!cap.isOpened())
    {
        std::cerr << "cannot open camera" << std::endl;
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return -1;
    }

    if (!capture_and_push_frames (cap, appsrc))
    {
        std::cerr << "failed during capture" << std::endl;
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);
    cap.release();

    return 0;

}

/*to compile - g++ fps_with_gst_display.cpp -o fps_with_gst_display `pkg-config --cflags --libs opencv4 gstreamer-1.0 gstreamer-app-1.0`*/