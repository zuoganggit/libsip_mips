#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unordered_map>
#include <sstream>
#include "audioStream.h"
#include "videoStream.h"
#include "sipSession.h"
#include "ctrlProtocol.h"
#include "configServer.h"

int sip_local_port = 5060;
#define  H264_PAYLOAD_TYPE 99
#define  G711U_PAYLOAD_TYPE 0


shared_ptr<SipSession> SipSession::GetInstance(const string& sipServerDomain,
    const string& userName,  const string& password){
    static auto sipPtr = make_shared<SipSession>(sipServerDomain, userName, password);
    return sipPtr;
}

SipSession::SipSession(const string &sipServerDomain,
                       const string &userName, const string &password)
{
    m_context_eXosip = nullptr;
    m_sip_server_domain = sipServerDomain;
    m_user_name = userName;
    m_password = password;
    m_exited = false;
    m_audio_rtp_local_port = 4002;
    m_video_rtp_local_port = 5002;
    m_is_calling = false;
    m_local_ip = "0.0.0.0";
    m_is_registed = false;
    m_CtrlProtocol_ptr = CtrlProtocol::GetInstance();
    m_AudioStream_ptr = AudioStream::GetInstance();
    m_VideoStream_ptr = VideoStream::GetInstance();
}

SipSession::~SipSession()
{
    cout<<"~SipSession() "<<endl;
    if(!m_exited){
        Stop();
    }

    if(m_context_eXosip){
        eXosip_quit(m_context_eXosip);
        osip_free(m_context_eXosip);
    }
}

bool SipSession::Start(){
    m_context_eXosip = eXosip_malloc();
    if(m_context_eXosip == nullptr){
        cout<<"eXosip_malloc fail"<<endl;
        return false;
    }
    if(eXosip_init(m_context_eXosip) < 0){
        cout<<"eXosip_init fail"<<endl;
        return false;
    }

    eXosip_set_user_agent(m_context_eXosip, "mips_sipper");
    string laddr = "0.0.0.0";
    ConfigServer::GetInstance()->GetLocalAddr(laddr);
    if(eXosip_listen_addr(m_context_eXosip, IPPROTO_UDP, laddr.c_str(), sip_local_port, AF_INET, 0) < 0){
        cout<<"eXosip_listen_addr fail"<<endl;
        if(eXosip_listen_addr(m_context_eXosip, IPPROTO_UDP, NULL, sip_local_port, AF_INET, 0) < 0){
            return false;
        }
    }else{
        m_local_ip = laddr;
    }

    int optval = 1;
    eXosip_set_option(m_context_eXosip, EXOSIP_OPT_AUTO_MASQUERADE_CONTACT, &optval);

    cout<<"authentication_info user "<< m_user_name << " password "<<m_password<<endl;
    if(eXosip_add_authentication_info(m_context_eXosip, m_user_name.c_str(), 
        m_user_name.c_str(), m_password.c_str(), NULL, NULL) < 0){
        cout<<"eXosip_add_authentication_info fail"<<endl;
        return false;
    }


    audio_rtp_session = make_shared<RtpSession>(m_audio_rtp_local_port, Audio, G711U_PAYLOAD_TYPE);
    video_rtp_session = make_shared<RtpSession>(m_video_rtp_local_port, Video, H264_PAYLOAD_TYPE);

    m_sip_run_future = std::async(std::launch::async, [this](){
        this->sipRun();
    });

    return true;
}


void SipSession::callAnswered(eXosip_event_t *event){
    osip_message_t *ack;
    if (0 == eXosip_call_build_ack(m_context_eXosip, event->did, &ack)){
        eXosip_call_send_ack(m_context_eXosip, event->did, ack);
        sdp_message_t * sdp = eXosip_get_remote_sdp(m_context_eXosip, event->did);
        if(sdp){
            m_CtrlProtocol_ptr->SendCallResult(DB_Result_Success);
            // char *str_sdp = NULL;
            // sdp_message_to_str(sdp, &str_sdp);
            // if(str_sdp){
            //     printf("sdp is %s\n", str_sdp);
            //     osip_free(str_sdp);
            // }
                
            sdp_connection_t *audioCon = eXosip_get_audio_connection(sdp);
            string remote_audio_addr = audioCon->c_addr;
            sdp_media_t * audioMedia = eXosip_get_audio_media(sdp);
            if(audioMedia){
                string remoteAudioPort = audioMedia->m_port;
                char *payloads = (char *)osip_list_get(&audioMedia->m_payloads, 0);
                cout<<"remote_audio_addr  "<<remote_audio_addr << " media "<< audioMedia->m_media<<
                    " remoteAudioPort "<<remoteAudioPort<< " payloads "<< payloads<<endl;
                
                int payload = std::atoi(payloads);
                audio_rtp_session->SetPayloadType(uint8_t(payload));

                audio_rtp_session->SetRemoteAddr(remote_audio_addr.c_str(), atoi(remoteAudioPort.c_str()));
                m_AudioStream_ptr->Open([this](uint8_t* data, int size){
                    // cout<<"pcm size "<<size<<endl;
                    audio_rtp_session->BuildRtpAndSend(data,size);
                });
            }

            sdp_connection_t *videoCon = eXosip_get_video_connection(sdp);
            string remote_video_addr = audioCon->c_addr;
            sdp_media_t * videoMedia = eXosip_get_video_media(sdp);
            m_call_did = event->did;
            if(videoMedia){
                string remoteVideoPort = videoMedia->m_port;

                char *payloads = (char *)osip_list_get(&videoMedia->m_payloads, 0);
                cout<<"remote_video_addr "<<remote_video_addr << " media "<< videoMedia->m_media<<
                    " remoteVideoPort "<<remoteVideoPort << "payloads "<<payloads <<endl;
                int payload = std::atoi(payloads);
                video_rtp_session->SetPayloadType(uint8_t(payload));

                video_rtp_session->SetRemoteAddr(remote_video_addr.c_str(), atoi(remoteVideoPort.c_str()));
                 m_VideoStream_ptr->Open([this](uint8_t* data, int size){
                    video_rtp_session->BuildRtpAndSend(data, size);
                });
            }
        }
        sdp_message_free(sdp);
        return;
    }
    m_CtrlProtocol_ptr->SendCallResult(DB_Result_Failed);
}


void SipSession::outCallAnswer(eXosip_event_t* event){
    osip_message_t *answer = NULL;

    sdp_message_t * sdp = eXosip_get_remote_sdp(m_context_eXosip, event->did);
    if(sdp){
        // char *str_sdp = NULL;
        // sdp_message_to_str(sdp, &str_sdp);
        // if(str_sdp){
        //     printf("sdp is %s\n", str_sdp);
        //     osip_free(str_sdp);
        // }
            
        sdp_connection_t *audioCon = eXosip_get_audio_connection(sdp);
        string remote_audio_addr = audioCon->c_addr;
        sdp_media_t * audioMedia = eXosip_get_audio_media(sdp);
        if(audioMedia){
            string remoteAudioPort = audioMedia->m_port;

            char *payloads = (char *)osip_list_get(&audioMedia->m_payloads, 0);
            cout<<"remote_audio_addr  "<<remote_audio_addr << " media "<< audioMedia->m_media<<
                  " remoteAudioPort "<<remoteAudioPort<< " payload "<<payloads<<endl;
            
            int payload = std::atoi(payloads);
            audio_rtp_session->SetPayloadType(uint8_t(payload));
            audio_rtp_session->SetRemoteAddr(remote_audio_addr.c_str(), atoi(remoteAudioPort.c_str()));
            m_AudioStream_ptr->Open([this](uint8_t* data, int size){
                    audio_rtp_session->BuildRtpAndSend(data, size);
                });
        }

        sdp_connection_t *videoCon = eXosip_get_video_connection(sdp);
        string remote_video_addr = audioCon->c_addr;
        sdp_media_t * videoMedia = eXosip_get_video_media(sdp);
        if(videoMedia){
            string remoteVideoPort = videoMedia->m_port;
            
            char *payloads = (char *)osip_list_get(&videoMedia->m_payloads, 0);
            cout<<"remote_video_addr "<<remote_video_addr << " media "<< videoMedia->m_media<<
                    " remoteVideoPort "<<remoteVideoPort << " video payloads "<<payloads <<endl;
            int payload = std::atoi(payloads);
            video_rtp_session->SetPayloadType(uint8_t(payload));


            video_rtp_session->SetRemoteAddr(remote_video_addr.c_str(), atoi(remoteVideoPort.c_str()));
            m_VideoStream_ptr->Open([this](uint8_t* data, int size){
                    video_rtp_session->BuildRtpAndSend(data, size);
                });
        }
    }
    sdp_message_free(sdp);
    m_call_did = event->did;
    int i = eXosip_call_build_answer(m_context_eXosip, event->tid, 200, &answer);
    if (i != 0)
    {
        cout<<"eXosip_call_build_answer fail "<<i<<endl;
        eXosip_call_send_answer(m_context_eXosip, event->tid, 400, NULL);
    }
    else
    {
        m_CtrlProtocol_ptr->SendCallResult(DB_Result_Success);
        char tmp[4096];
        char localip[128] = {0};
        if(m_local_ip == "0.0.0.0"){
            eXosip_guess_localip(m_context_eXosip, AF_INET, localip, 128);
        }else{
            strcpy(localip, m_local_ip.c_str());
        }
        

        snprintf (tmp, 4096,
                "v=0\r\n"
                "o=jack 0 0 IN IP4 %s\r\n"
                "s=conversation\r\n"
                "c=IN IP4 %s\r\n"
                "t=0 0\r\n"
                "m=audio %d RTP/AVP 0 8\r\n"
                "a=rtpmap:0 PCMU/8000\r\n"
                "a=rtpmap:8 PCMA/8000\r\n"

                "m=video %d RTP/AVP 99 \r\n"
                "a=rtpmap:99 H264/90000 \r\n"
                "a=fmtp:99  packetization-mode=1 \r\n"
                // profile-level-id=42e01e;
                // "a=sendonly \r\n"
                // "a=ptime:40 \r\n"
                , localip, localip, m_audio_rtp_local_port, m_video_rtp_local_port);

        osip_message_set_body (answer, tmp, strlen (tmp));
        osip_message_set_content_type (answer, "application/sdp");

        eXosip_call_send_answer(m_context_eXosip, event->tid, 200, answer);
        m_is_calling = true;
    }
}


std::unordered_map<std::string, std::string> parseKeyValuePairs(const std::string& input) {
    std::unordered_map<std::string, std::string> keyValuePairs;
    std::istringstream iss(input);

    std::string line;
    while (std::getline(iss, line)) {
        std::istringstream lineStream(line);
        std::string key, value;
        if (std::getline(lineStream, key, '=') && std::getline(lineStream, value)) {
            keyValuePairs[key] = value;
        }
    }

    return keyValuePairs;
}

void SipSession::sipRun(){
    int rid = 0;    
    string from = "sip:" + m_user_name + "@" + m_sip_server_domain;
    string to = "sip:" + m_sip_server_domain;
    
    cout<<"prepare register from "<<from << " to "<< to<<endl;
    osip_message_t *reg = nullptr;
    
    uint16_t tt = 0;
    int open_mutex_channel = -1;
    int register_fail_count = 0;
    while(!m_exited){
        if(tt % 30 == 0){
            eXosip_lock(m_context_eXosip);
            if(rid > 0){
                eXosip_register_remove(m_context_eXosip, rid);
            }

            rid = eXosip_register_build_initial_register(m_context_eXosip, from.c_str(), to.c_str(), NULL, 1800, &reg);
            if (rid < 0)
            {
                printf("eXosip_register_build_initial_register fail %d\n", rid);
            }else{
                eXosip_register_send_register(m_context_eXosip, rid, reg);
            }
            eXosip_unlock(m_context_eXosip);
        }

        tt++;
        osip_message_t *answer = NULL;
        osip_body *body = nullptr;
        eXosip_event_t *event = eXosip_event_wait(m_context_eXosip, 1, 0);
        if(event){
            eXosip_lock(m_context_eXosip);
            eXosip_automatic_action(m_context_eXosip);
            // cout<<"SIP EVENT "<<event->textinfo<<" type "<< event->type<<endl;

            switch(event->type){
                case EXOSIP_REGISTRATION_SUCCESS:
                    m_is_registed = true;
                    register_fail_count = 0;
                    break;
                case EXOSIP_REGISTRATION_FAILURE:
                    register_fail_count++;
                    if(register_fail_count > 1){
                        m_is_registed = false;
                        register_fail_count = 0;
                    }
                    break;
                case EXOSIP_CALL_INVITE:
                    if(m_AudioStream_ptr->IsOpened()){
                        eXosip_call_send_answer(m_context_eXosip, event->tid, 
                            SIP_BUSY_HERE, NULL);
                        m_CtrlProtocol_ptr->SendCallResult(DB_Result_Busy);
                    }else{
                        m_CtrlProtocol_ptr->SendCallResult(DB_Result_Calling);
                        outCallAnswer(event);
                    }
                    break;
                case EXOSIP_CALL_REINVITE:
                    // AudioStream::GetInstance()->Close();
                    // VideoStream::GetInstance()->Close();
                    m_CtrlProtocol_ptr->SendCallResult(DB_Result_Calling);
                    outCallAnswer(event);
                    break;
                case EXOSIP_CALL_REQUESTFAILURE:
                    m_CtrlProtocol_ptr->SendCallResult(DB_Result_Failed);
                    break;
                case EXOSIP_CALL_MESSAGE_NEW:
                    // osip_message_to_str(event->request, &message_str, &message_size);
                    osip_message_get_body(event->request, 0, &body);
                    if(body){
                        unordered_map<string, string> keyValuePairs = parseKeyValuePairs(body->body);
                        if (keyValuePairs.count("Signal") > 0) {
                            string signalValue = keyValuePairs["Signal"];

                            if(signalValue.find("#") != string::npos && open_mutex_channel > 0){
                                printf("open mutex  channel %d\n", open_mutex_channel);
                                m_CtrlProtocol_ptr->OpenMutex(open_mutex_channel);
                                open_mutex_channel = 0;
                            }else{
                                open_mutex_channel = std::atoi(signalValue.c_str());
                            }                            
                        }
                    }
                    break;
                case EXOSIP_CALL_PROCEEDING:
                    break;
                case EXOSIP_CALL_ANSWERED:
                    callAnswered(event);
                    break;
                case EXOSIP_CALL_CLOSED:
                    if(event->did == m_call_did){
                        m_AudioStream_ptr->Close();
                        m_VideoStream_ptr->Close();
                        m_is_calling = false;
                    }
                    m_CtrlProtocol_ptr->SendCallResult(DB_Result_HangUp);
                    break;
                case EXOSIP_CALL_RELEASED:

                    break;
                default:
                    break;
            }
            eXosip_event_free(event);
            eXosip_unlock(m_context_eXosip);
        }
    }

}
bool SipSession::Stop(){
    TerminateCalling();
    m_exited = true;
    m_sip_run_future.wait();
    return true;
}

bool SipSession::CallOutgoing(const string &toUser){
    if(m_is_calling){
        cerr<<"is calling ,please close call"<<endl;
        return false;
    }
    if(!m_exited){
        osip_message_t *invite;
        int cid;
        string to = "<sip:" + toUser + "@"+m_sip_server_domain+">";
        string from = "<sip:" + m_user_name + "@"+m_sip_server_domain+">";
        int i = eXosip_call_build_initial_invite (m_context_eXosip, &invite, to.c_str(),
                                            from.c_str(),
                                            NULL, // optional route header
                                            "This is a call for a conversation");
        if (i != 0)
        {
            printf("eXosip_call_build_initial_invite fail\n");
            return false;
        }

        char tmp[4096];
        char localip[128];

        if(m_local_ip == "0.0.0.0"){
            eXosip_guess_localip(m_context_eXosip, AF_INET, localip, 128);
        }else{
            strcpy(localip, m_local_ip.c_str());
        }

        snprintf (tmp, 4096,
                "v=0\r\n"
                "o=jack 0 0 IN IP4 %s\r\n"
                "s=conversation\r\n"
                "c=IN IP4 %s\r\n"
                "t=0 0\r\n"
                "m=audio %d RTP/AVP 0 8\r\n"
                "a=rtpmap:0 PCMU/8000\r\n"
                "a=rtpmap:8 PCMA/8000\r\n"
                "m=video %d RTP/AVP 99 \r\n"
                "a=rtpmap:99 H264/90000 \r\n"
                "a=fmtp:99 packetization-mode=1 \r\n"
                // "a=sendonly \r\n"
                // "a=ptime:40 \r\n"
                , localip, localip, m_audio_rtp_local_port, m_video_rtp_local_port);
                // "a=rtpmap:101 telephone-event/8000\r\n"
                // "a=fmtp:101 0-11\r\n", localip, localip, 3000);
        osip_message_set_body (invite, tmp, strlen(tmp));
        osip_message_set_content_type (invite, "application/sdp");

        eXosip_lock(m_context_eXosip);
        cid = eXosip_call_send_initial_invite (m_context_eXosip, invite);
        eXosip_unlock(m_context_eXosip);
        if(cid < 0){
            printf("eXosip_call_send_initial_invite fail\n");
            return false;
        }
    }

    printf("CallOutgoing ok\n");
    m_CtrlProtocol_ptr->SendCallResult(DB_Result_Calling);
    return true;
}
int SipSession::TerminateCalling(){
    eXosip_lock(m_context_eXosip);
    eXosip_call_terminate(m_context_eXosip, m_call_cid, m_call_did);
    eXosip_unlock(m_context_eXosip);
    m_AudioStream_ptr->Close();
    m_VideoStream_ptr->Close();
    m_is_calling = false;
    return 0;
}

bool SipSession::GetRegStatus(){
    return m_is_registed;
}
void SipSession::openMutexCtl(int channel){
    m_CtrlProtocol_ptr->OpenMutex(channel);
}