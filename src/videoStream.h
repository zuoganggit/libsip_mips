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
    // void WriteVideoFrame(uint8_t data, int dataSize);
private:
    void run();
    bool m_opening;
    FrameCallback m_frame_callback;
    int m_t21_port;
    std::future<void> m_stream_run_future;
};

#endif