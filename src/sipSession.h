#ifndef __SIP_SESSION__
#define __SIP_SESSION__
#include <osip2/osip_mt.h>
#include <eXosip2/eXosip.h>
#include <iostream>
#include <future>
#include <memory>
#include "rtpSession.h"

using namespace std;

class SipSession{
public:
    static shared_ptr<SipSession> GetInstance(const string& sipServerDomain = "",
    const string& userName="",  const string& password="");
    SipSession(const string& sipServerDomain,
    const string& userName,  const string& password);
    ~SipSession();

    bool Start();
    bool Stop();

    int CallOutgoing(const string& toUser);
    int TerminateCalling();
private:
    void openMutexCtl(int channel);
    void sipRun();
    void callAnswered(eXosip_event_t *event); 
    void callByed(eXosip_event_t *event);

    void outCallAnswer(eXosip_event_t* event);

    void sendAudioRtp(uint8_t* frame, int size);
    struct eXosip_t *m_context_eXosip;
    string m_local_ip;
    string m_sip_server_domain;
    string m_user_name;
    string m_password;
    bool m_exited;
    std::future<void> m_sip_run_future;
    int m_call_cid;
    int m_call_did;
    bool m_is_calling;
    int m_audio_rtp_local_port;
    int m_video_rtp_local_port;
    shared_ptr<RtpSession> audio_rtp_session;
    shared_ptr<RtpSession> video_rtp_session;
};

#endif