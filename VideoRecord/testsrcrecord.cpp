 #include <signal.h>
#include <iostream>
#include <boost/circular_buffer.hpp>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>

/* Some static variables that would be used from sig handler */
static int width = 0;
static int height = 0;
static float fps = 0.f;
static int cur_frame_nb = -1;
static int last_record_frame_nb = -1;
static int cur_video_nb = -1;
static cv::VideoWriter* writer = NULL;


/* Create a video writer with gstreamer encoding from opencv BGR format with H264 into MP4 container file */
cv::VideoWriter * createVideoWriter() {
    char writer_pipeline_str[128] = {0};
    sprintf(writer_pipeline_str, "appsrc ! video/x-raw,format=BGR ! videoconvert ! x264enc ! h264parse ! qtmux ! filesink location=video_%02d.mp4", cur_video_nb);
    cv::VideoWriter* writer = new cv::VideoWriter(writer_pipeline_str, cv::CAP_GSTREAMER, 0, fps, cv::Size(width, height));
    return writer;
}


/* Signal handler that would asynchronously start a video capture for 7s */
void my_sighandler(int s){
    if (writer) {
       std::cerr << "Video recording already in process, ignoring" << std::endl;
       return;
    }
    
    printf("Start recording video %02d\n", ++cur_video_nb);
    writer = createVideoWriter();
    last_record_frame_nb = cur_frame_nb + (int)(7*fps);
}


int
main ()
{
    /* You would need an opencv build with gstreamer backend support. You can check uncommenting the following line */
    // std::cout << cv::getBuildInformation() << std::endl; 

    /* Install handler for catching Ctrl-C that would create a videoWriter encoding H264 and putting into a MP4 file for 7s */
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = my_sighandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);

    /* Create a capture from H264 simulated camera */
    const char* cap_str = "videotestsrc pattern=ball is-live=1 do-timestamp=1 ! videoconvert ! video/x-raw,format=I420,width=320,height=240,framerate=30/1 ! x264enc ! queue name=cam\
        cam. ! video/x-h264,width=320,height=240,framerate=30/1 ! h264parse ! avdec_h264 ! videoconvert ! queue ! video/x-raw,format=BGR ! appsink drop=1";
    cv::VideoCapture cap(cap_str, cv::CAP_GSTREAMER);
    if (!cap.isOpened ()) {
      std::cout << "Failed to open camera." << std::endl;
      return (-1);
    
    }
    std::cout << "Video Capture opened (backend: " << cap.getBackendName() << ")" << std::endl;
    width = (int) cap.get (cv::CAP_PROP_FRAME_WIDTH);
    height = (int) cap.get (cv::CAP_PROP_FRAME_HEIGHT);
    fps = cap.get (cv::CAP_PROP_FPS);
    std::cout << "Framing: " << width << " x " << height << " @" << fps << " FPS" <<std::endl;
 
    unsigned int bufferDepth = (unsigned int)(7 * fps);
    boost::circular_buffer<cv::Mat> cb(bufferDepth);
 
    /* Loop until error or if you type 'q' with focus on the opencv GUI display window */
    cv::Mat frame_in, frame_out;
    while (1) {
       /* Read frame from capture */
       if (!cap.read (frame_in)) {
          std::cout << "Capture read error" << std::endl;
          break;
       }
       ++cur_frame_nb;
       cv::imshow("IPCam monitor",frame_in);
       cb.push_back(frame_in.clone());
       if(cb.size() == bufferDepth) {
           frame_out = cb.front();
           cb.pop_front();
           /* Manage writer if currently active */
           if (writer) {
               if (cur_frame_nb <= last_record_frame_nb)
                   writer->write(frame_out);
               else {
                   std::cout << "Stopping recording video " << cur_video_nb << std::endl;
                   writer->release();
                   delete writer;
                   writer = NULL;
               }
           }
           cv::imshow("Delayed output",frame_out);
       }
       if ('q' == cv::waitKey(1))
           break;
   }
   if(writer)
       writer->release();
   cap.release ();

   return 0;
}
