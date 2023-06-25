#ifndef __CTRL_PROTOCOL__
#define __CTRL_PROTOCOL__

#include <functional>
#include <memory>
#include <future>
extern "C"{
    #include <netinet/in.h>
}
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
    void OpenVideoChannel();
    void CloseAudioChannel();
    void CloseVideoChannel();
    void CallNotify(int acount_index);
    void Init();
private:
    bool openUdpSocket();
    void closeUdpSocket();
    void run();

    int m_t21_socket;
    sockaddr_in m_destinationAddr;
    bool m_exited;
    std::future<void> m_run_future;
};

#endif