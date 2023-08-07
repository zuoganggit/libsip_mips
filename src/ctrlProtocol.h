#ifndef __CTRL_PROTOCOL__
#define __CTRL_PROTOCOL__

#include <functional>
#include <memory>
#include <future>
extern "C"{
    #include <netinet/in.h>
}
#include "T21_Def.h"
using namespace::std;

typedef std::function<void (uint8_t* data, int size)> FrameCallback;



class CtrlProtocol{
public:
    #define T21_PORT    15601
    #define LOCAL_PORT  15611
    CtrlProtocol();
    ~CtrlProtocol();

    static shared_ptr<CtrlProtocol> GetInstance();
    void OpenMutex(int channel);
    void OpenAudioChannel();

    void OpenAudioInChannel(AudioEncodeType_e audioType);
    void OpenAudioOutChannel(AudioEncodeType_e audioType);


    void OpenVideoChannel();
    void CloseAudioChannel();
    void CloseVideoChannel();
    void CallOutgoing(T21_Data *data);
    void StopOutgoing(T21_Data *data);
    void SendRegStatus(T21_Data *data);
    void SendCallResult(Result_e ret);

    void TunnelDataHandle(T21_Data *data);
    void SendTunnelData(uint8_t* data, int size);

    void DirectCall(T21_Data *data);
    void AnswerCall(T21_Data *data);
    void Init();
private:
    bool openUdpSocket();
    void closeUdpSocket();
    void run();
    void t21CmdHandle(T21_Data *data);

    int m_t21_socket;
    sockaddr_in m_destinationAddr;
    bool m_exited;
    std::future<void> m_run_future;
};

#endif