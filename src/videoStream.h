#ifndef __VIDEO_STREAM__
#define __VIDEO_STREAM__
#include <future>
#include <memory>
#include "ctrlProtocol.h"
using namespace std;

class VideoStream{
public:
    VideoStream(int t21port);
    ~VideoStream();
    static shared_ptr<VideoStream> GetInstance(int t21port = 15602);
    bool Init();
    void Open(FrameCallback callback);
    bool IsOpened();
    void Close();
private:
    void run();
    void run_new();
    bool m_opening;
    FrameCallback m_frame_callback;
    int m_socket;    
    std::future<void> m_stream_run_future;
};

#endif