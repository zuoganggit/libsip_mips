#ifndef __AUDIO_STREAM__
#define __AUDIO_STREAM__
#include "ctrlProtocol.h"
#include <future>
#include <memory>
using namespace std;

class AudioStream{
public:
    AudioStream(int t21port);
    ~AudioStream();
    static shared_ptr<AudioStream> GetInstance(int t21port = 15603);
    bool Init();
    void Open(FrameCallback callback);
    bool IsOpened();
    void Close();
    void WriteAudioFrame(uint8_t data, int dataSize);
private:
    void run_test();
    void run();
    bool m_opening;
    FrameCallback m_frame_callback;
    int m_t21_port;
    std::future<void> m_stream_run_future;
    int m_socket;
};


#endif