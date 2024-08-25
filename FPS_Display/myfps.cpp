/*Display stream with FPS 
capture the frames using a gst pipeline and pass it to the appsink
add the fps to it and display it using OpenCV imshow*/


#include <iostream>
#include <gst/gst.h>
#include <opencv2/opencv.hpp>
#include <chrono>

//func to calculate fps
double calcFPS()
{
    static int frameCount = 0;
    static double lastTime = (double)cv::getTickCount();
    printf("lastime = %.3f\n", lastTime/CLOCKS_PER_SEC);
    static double fps=0;

    frameCount++;
    if(frameCount >= 1)//calc evry frames
    {
        double currentTime = (double)cv::getTickCount();
        printf("current = %.3f\n", currentTime/CLOCKS_PER_SEC);
        fps = frameCount / ((currentTime - lastTime)/ cv::getTickFrequency());
        lastTime = currentTime;
        frameCount = 0;
    }
    return fps;

}

int main(int argc, char** argv)
{
    double fps;
    gst_init(&argc, &argv);

    //pipeline to capture the raw frames from the camera
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

        fps = calcFPS();// apply the fps to the frames

        cv::putText(frame, "FPS:" + std::to_string(fps), cv::Point(30, 50), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 0, 255), 2);
        cv::imshow("feed", frame); //display the frames

        if(cv::waitKey(1) == 'q')
        {
            break;
        }

    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}

/*to compile - g++ myfps.cpp -o myfps `pkg-config --cflags --libs opencv4 gstreamer-1.0 gstreamer-app-1.0`*/