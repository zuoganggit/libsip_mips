#ifndef __RTP_SESSION__
#define __RTP_SESSION__
#include "ctrlProtocol.h"
#include "audioStream.h"
#include <iostream>
#include <future>
extern "C"{
    #include <netinet/in.h>
}
using namespace std;

typedef enum{
    Audio,
    Video
}RtpSessionType;
class RtpSession{
public:
    RtpSession(int localPort, RtpSessionType type, uint8_t payloadType, FrameCallback readAudioData = nullptr);
    ~RtpSession();
    bool Start();
    bool Stop();
    void SetRemoteAddr(const string& dstAddr, int dstPort);
    void SetPayloadType(uint8_t payloadType);
    void BuildRtpAndSend(const uint8_t* payload, size_t payloadSize);
private:
    void h264_Frame_RtpSend(const uint8_t* frameData, int frameSize);
    void h264_Frag_fu_a(uint8_t *nal, int fragsize, bool mark);
    void run();
    string m_local_addr;
    string m_remote_addr;
    int m_remote_port;
    int m_local_port;

    bool m_opening;
    std::future<void> m_run_future;
    RtpSessionType m_session_type;
    FrameCallback m_audio_data_callback;
    int m_socket;
    sockaddr_in m_destinationAddr;

    uint16_t m_sequenceNumber;
    uint32_t m_timestamp;
    uint8_t m_payloadType;
    bool m_mark;
    bool m_is_talk;

    shared_ptr<AudioStream> m_audio_ptr; 
    shared_ptr<CtrlProtocol> m_CtrlProtocol_ptr;
};


#endif