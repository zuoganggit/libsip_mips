#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <regex>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "audioStream.h"
#include "videoStream.h"
#include "sipSession.h"
#include "ctrlProtocol.h"
#include "configServer.h"

int sip_local_port = 5060;
#define  H264_PAYLOAD_TYPE 99
#define  G711U_PAYLOAD_TYPE 0
#define  G711A_PAYLOAD_TYPE 8
#define  L16_PAYLOAD_TYPE 98



struct RtpmapInfo {
    int payload;
    std::string encoding;
    int sampleRate;
};

bool isPortListening(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("Error creating socket");
        return false;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        close(sockfd);
        return false;
    }

    close(sockfd);
    return true;
}


// SDP解析函数
std::vector<RtpmapInfo> parseSDP(const std::string& sdp) {
    std::vector<RtpmapInfo> rtpmapList;

    // 正则表达式匹配RTPMAP字段
    std::regex rtpmap_regex("a=rtpmap:(\\d+) ([^\\s/]+)/([0-9]+)");
    std::smatch matches;

    // 使用迭代器遍历匹配
    std::string::const_iterator searchStart(sdp.cbegin());
    while (std::regex_search(searchStart, sdp.cend(), matches, rtpmap_regex)) {
        // 提取并保存RTPMAP信息
        RtpmapInfo info;
        info.payload = std::stoi(matches[1]);
        info.encoding = matches[2];
        info.sampleRate = std::stoi(matches[3]);
        rtpmapList.push_back(info);

        // 继续搜索下一个匹配
        searchStart = matches.suffix().first;
    }

    return rtpmapList;
}


AudioEncodeType_e getT21AudioType(string codecStr){
    if(codecStr == "L16"){
        return ET_PCM;
    }else if(codecStr == "PCMU"){
        return ET_G711U;
    }else if(codecStr == "PCMA"){
        return ET_G711A;
    }

    return ET_G711U;
}



static int _am_option_route_add_lr(const char *orig_route, char *dst_route, int dst_route_size) {
  osip_route_t *route_header = NULL;
  char *new_route = NULL;
  const char *tmp;
  const char *tmp2;
  int i;

  memset(dst_route, '\0', dst_route_size);
  if (orig_route == NULL || orig_route[0] == '\0')
    return 0;

  tmp = strstr(orig_route, "sip:");
  tmp2 = strstr(orig_route, "sips:");
  if (tmp == NULL && tmp2 == NULL) {
    snprintf(dst_route, dst_route_size, "<sip:%s;lr>", orig_route);
    return 0;
  }

  i = osip_route_init(&route_header);
  if (i != 0 || route_header == NULL)
    return OSIP_NOMEM;
  i = osip_route_parse(route_header, orig_route);
  if (i != 0 || route_header->url == NULL || route_header->url->host == NULL) {
    osip_route_free(route_header);
    snprintf(dst_route, dst_route_size, "%s", orig_route);
    return i;
  }

  tmp = strstr(orig_route, ";lr");
  if (tmp == NULL)
    osip_uri_uparam_add(route_header->url, osip_strdup("lr"), NULL);

  i = osip_route_to_str(route_header, &new_route);
  osip_route_free(route_header);
  if (i != 0 || new_route == NULL) {
    return i;
  }
  snprintf(dst_route, dst_route_size, "%s", new_route);
  osip_free(new_route);
  return 0;
}

static int _prepend_route(osip_message_t *sip, const char *hvalue) {
  osip_route_t *route;
  int i;
  char outbound_route[256];

  if (hvalue == NULL || hvalue[0] == '\0')
    return OSIP_SUCCESS;

  memset(outbound_route, '\0', sizeof(outbound_route));
  i = _am_option_route_add_lr(hvalue, outbound_route, sizeof(outbound_route));
  if (i != 0)
    return i;

  i = osip_route_init(&route);

  if (i != 0)
    return i;
  i = osip_route_parse(route, outbound_route);
  if (i != 0) {
    osip_route_free(route);
    return i;
  }
  sip->message_property = 2;
  osip_list_add(&sip->routes, route, 0);
  return OSIP_SUCCESS;
}


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
}

SipSession::~SipSession()
{
    cout<<"~SipSession() "<<endl;
    if(!m_exited && m_context_eXosip){
        Stop();
    }

    if(m_context_eXosip){
        eXosip_quit(m_context_eXosip);
        osip_free(m_context_eXosip);
    }
}

bool SipSession::Start(){
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_family = AF_INET;

    int pos = m_sip_server_domain.find(":");
    if(pos != string::npos){
        printf("domain %s\n", m_sip_server_domain.substr(0, pos).c_str());
        int err = getaddrinfo(m_sip_server_domain.substr(0, pos).c_str(), NULL, &hints, &result);
        if(err != 0){
            printf(" getaddrinfo %d,  %s\n", err, gai_strerror(err));
            return false;
        }
    }

    if(isPortListening(sip_local_port)){
        cout<<"udp port "<<sip_local_port<<" has listening"<<endl;
        return false;
    }

    m_CtrlProtocol_ptr = CtrlProtocol::GetInstance();
    m_AudioStream_ptr = AudioStream::GetInstance();
    m_VideoStream_ptr = VideoStream::GetInstance();

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
    int trans_type = IPPROTO_UDP;

    int optval = 1;
    eXosip_set_option(m_context_eXosip, EXOSIP_OPT_AUTO_MASQUERADE_CONTACT, &optval);
    if(ConfigServer::GetInstance()->GetEnableSipTcp()){
        trans_type = IPPROTO_TCP;
        eXosip_set_option(m_context_eXosip, EXOSIP_OPT_ENABLE_REUSE_TCP_PORT, &optval);
        // int val=10000;
	    // eXosip_set_option(m_context_eXosip, EXOSIP_OPT_KEEP_ALIVE_OPTIONS_METHOD, (void*)&val);
        eXosip_set_option(m_context_eXosip, EXOSIP_OPT_ENABLE_OUTBOUND, (void*)&optval);
    }

    if(eXosip_listen_addr(m_context_eXosip, trans_type, laddr.c_str(), sip_local_port, AF_INET, 0) != 0){
        cout<<"eXosip_listen_addr fail"<<endl;
        if(eXosip_listen_addr(m_context_eXosip, trans_type, NULL, sip_local_port, AF_INET, 0) != 0){
            cout<<"eXosip_listen_addr fail, SipSession start fail"<<endl;
            return false;
        }
    }else{
        m_local_ip = laddr;
    }

    // int optval = 1;
    // eXosip_set_option(m_context_eXosip, EXOSIP_OPT_AUTO_MASQUERADE_CONTACT, &optval);
    //EXOSIP_OPT_USE_RPORT
    // eXosip_set_option(m_context_eXosip, EXOSIP_OPT_ENABLE_REUSE_TCP_PORT, &optval);

    cout<<"authentication_info user "<< m_user_name << " password "<<m_password<<endl;
    if(eXosip_add_authentication_info(m_context_eXosip, m_user_name.c_str(), 
        m_user_name.c_str(), m_password.c_str(), NULL, NULL) < 0){
        cout<<"eXosip_add_authentication_info fail"<<endl;
        // return false;
    }


    audio_rtp_session = make_shared<RtpSession>(m_audio_rtp_local_port, Audio, G711U_PAYLOAD_TYPE);
    video_rtp_session = make_shared<RtpSession>(m_video_rtp_local_port, Video, H264_PAYLOAD_TYPE);

    m_sip_run_future = std::async(std::launch::async, [this](){
        this->sipRun();
    });

    return true;
}


void SipSession::callAckAnswered(eXosip_event_t *event){
    osip_message_t *ack;
    if (0 == eXosip_call_build_ack(m_context_eXosip, event->tid, &ack)){
        eXosip_call_send_ack(m_context_eXosip, event->tid, ack);
        sdp_message_t * sdp = eXosip_get_remote_sdp(m_context_eXosip, event->did);
        Codec audioCodec = G711U;
        string audioStr = "PCMU";
        int audioPayloadType = 0;
        int telePayloadType = 101;
        
        if(sdp){
            m_CtrlProtocol_ptr->SendCallResult(DB_Result_Success);
            
            char *str_sdp = NULL;
            sdp_message_to_str(sdp, &str_sdp);
            if(str_sdp){
                printf("sdp %s\n", str_sdp);
            }
            sdp_connection_t *audioCon = eXosip_get_audio_connection(sdp);
            string remote_audio_addr = audioCon->c_addr;
            sdp_media_t * audioMedia = eXosip_get_audio_media(sdp);
            if(audioMedia){

                auto rtpmap_list = parseSDP(str_sdp);
                for(auto i:rtpmap_list){
                    cout << "rtpmap encoding "<<i.encoding<<" payload "<<
                        i.payload<< " sampleRate "<<i.sampleRate<<endl;
                    if(i.encoding == "telephone-event"){
                        telePayloadType = i.payload;
                    }
                }
                bool foundCodec = false;
                auto local_codecs = ConfigServer::GetInstance()->GetAudioCodec();
                //协商找出双方都支持的编码格式，如果不支持就设置为默认PCMU
                for(auto i:rtpmap_list){
                    for(auto c:local_codecs){
                        cout<<"local codec string "<< ConfigServer::GetInstance()->CodecString(c)
                            << " remote codec string  "<<i.encoding<<endl;

                        if(i.encoding == 
                            ConfigServer::GetInstance()->CodecString(c)){
                            //匹配
                            audioCodec = c;
                            audioStr = i.encoding;
                            audioPayloadType = i.payload;
                            foundCodec = true;
                            break;
                        }
                    }
                    if(foundCodec) break;
                }

                string remoteAudioPort = audioMedia->m_port;
                char *payloads = (char *)osip_list_get(&audioMedia->m_payloads, 0);
                cout<<"remote_audio_addr  "<<remote_audio_addr << " media "<< audioMedia->m_media<<
                    " remoteAudioPort "<<remoteAudioPort<< " payload "<< payloads<<endl;
                
                // int payload = std::atoi(payloads);
                audio_rtp_session->SetPayloadType(audioPayloadType);
                audio_rtp_session->SetCodecType(getT21AudioType(audioStr));
                audio_rtp_session->SetRemoteAddr(remote_audio_addr.c_str(), atoi(remoteAudioPort.c_str()));
                m_AudioStream_ptr->Open([this](uint8_t* data, int size){
                    // cout<<"pcm size "<<size<<endl;
                    audio_rtp_session->BuildRtpAndSend(data,size);
                }, getT21AudioType(audioStr), getT21AudioType(audioStr));
            }

            sdp_connection_t *videoCon = eXosip_get_video_connection(sdp);
            string remote_video_addr = audioCon->c_addr;
            sdp_media_t * videoMedia = eXosip_get_video_media(sdp);
            m_call_did = event->did;
            if(videoMedia && atoi(videoMedia->m_port) > 0){
                string remoteVideoPort = videoMedia->m_port;

                char *payloads = (char *)osip_list_get(&videoMedia->m_payloads, 0);
                cout<<"remote_video_addr "<<remote_video_addr << " media "<< videoMedia->m_media<<
                    " remoteVideoPort "<<remoteVideoPort << "payload "<<payloads <<endl;
                int payload = std::atoi(payloads);

                video_rtp_session->SetPayloadType(payload);

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

void SipSession::outCallAnswer(int did, int tid){
    osip_message_t *answer = NULL;

    int i = eXosip_call_build_answer(m_context_eXosip, tid, 200, &answer);
    if (i != 0)
    {
        cout<<"eXosip_call_build_answer fail "<<i<<endl;
        eXosip_call_send_answer(m_context_eXosip, tid, 400, NULL);
        m_is_calling = false;
        return;
    }

    sdp_message_t * sdp = eXosip_get_remote_sdp(m_context_eXosip, did);
    int video_rtp_type = 99;
    int audio_rtp_type = 0;

    Codec audioCodec = G711U;
    string audioStr = "PCMU";
    int audioPayloadType = 0;
    int telePayloadType = 101;
    int remote_video_port = 0;
    if(sdp){
        char *str_sdp = NULL;
        sdp_message_to_str(sdp, &str_sdp);
        printf("str_sdp  %s\n", str_sdp);
        sdp_connection_t *audioCon = eXosip_get_audio_connection(sdp);
        string remote_audio_addr = audioCon->c_addr;
        sdp_media_t * audioMedia = eXosip_get_audio_media(sdp);
        if(audioMedia){
            auto rtpmap_list = parseSDP(str_sdp);
            for(auto i:rtpmap_list){
                cout << "rtpmap encoding "<<i.encoding<<" payload "<<
                    i.payload<< " sampleRate "<<i.sampleRate<<endl;
                if(i.encoding == "telephone-event"){
                    telePayloadType = i.payload;
                }
            }
            bool foundCodec = false;
            auto local_codecs = ConfigServer::GetInstance()->GetAudioCodec();
            //协商找出双方都支持的编码格式，如果不支持就设置为默认PCMU
            for(auto i:rtpmap_list){
                for(auto c:local_codecs){

                    cout<<"local codec string "<< ConfigServer::GetInstance()->CodecString(c)
                        << " remote codec string  "<<i.encoding<<endl;

                    if(i.encoding == 
                        ConfigServer::GetInstance()->CodecString(c)){
                        //匹配
                        audioCodec = c;
                        audioStr = i.encoding;
                        audioPayloadType = i.payload;
                        foundCodec = true;
                        break;
                    }
                }
                if(foundCodec) break;
            }

            string remoteAudioPort = audioMedia->m_port;
            char *payloads = (char *)osip_list_get(&audioMedia->m_payloads, 0);
            cout<<"Answer Invite remote_audio_addr  "<<remote_audio_addr << " media "<< audioMedia->m_media<<
                  " remoteAudioPort "<<remoteAudioPort<< " payload "<<payloads<<endl;
            
            // audio_rtp_type = std::atoi(payloads);
            audio_rtp_session->SetPayloadType(audioPayloadType, telePayloadType);
            audio_rtp_session->SetCodecType(getT21AudioType(audioStr));
            audio_rtp_session->SetRemoteAddr(remote_audio_addr.c_str(), atoi(remoteAudioPort.c_str()));
            m_AudioStream_ptr->Open([this](uint8_t* data, int size){
                    audio_rtp_session->BuildRtpAndSend(data, size);
                }, getT21AudioType(audioStr), getT21AudioType(audioStr));
        }

        sdp_connection_t *videoCon = eXosip_get_video_connection(sdp);
        string remote_video_addr = audioCon->c_addr;
        sdp_media_t * videoMedia = eXosip_get_video_media(sdp);
        
        if(videoMedia && atoi(videoMedia->m_port) > 0){
            remote_video_port = atoi(videoMedia->m_port);
            string remoteVideoPort = videoMedia->m_port;
            
            char *payloads = (char *)osip_list_get(&videoMedia->m_payloads, 0);
            cout<<"Answer Invite remote_video_addr "<<remote_video_addr << " media "<< videoMedia->m_media<<
                    " remoteVideoPort "<<remoteVideoPort << " video payloads "<<payloads <<endl;
            video_rtp_type = std::atoi(payloads);
            video_rtp_session->SetPayloadType(uint8_t(video_rtp_type));

            video_rtp_session->SetRemoteAddr(remote_video_addr.c_str(), atoi(remoteVideoPort.c_str()));
            m_VideoStream_ptr->Open([this](uint8_t* data, int size){
                    video_rtp_session->BuildRtpAndSend(data, size);
                });
        }
    }

    sdp_message_free(sdp);
    m_call_did = did;

    m_CtrlProtocol_ptr->SendCallResult(DB_Result_Success);
    char tmp[4096];
    char localip[128] = {0};
    if(m_local_ip == "0.0.0.0"){
        eXosip_guess_localip(m_context_eXosip, AF_INET, localip, 128);
    }else{
        strcpy(localip, m_local_ip.c_str());
    }

    char video_tmp[1024] = {0};
    if(remote_video_port > 0)
    {
        snprintf(video_tmp, 1024, 
        "m=video %d RTP/AVP %d\r\n"
        "a=rtpmap:%d H264/90000\r\n"
        "a=fmtp:%d packetization-mode=1\r\n"
        // "a=ssrc:16909060\r\n"
        // profile-level-id=428015
        "a=sendrecv\r\n"
        , m_video_rtp_local_port, 
        video_rtp_type, video_rtp_type, video_rtp_type);
    }else{
        snprintf(video_tmp, 1024, 
        "m=video 0 RTP/AVP 0\r\n"
        "a=label:11\r\n"
        "a=content:main\r\n"
        );
    }
    snprintf (tmp, 4096,
            "v=0\r\n"
            "o=jack 0 0 IN IP4 %s\r\n"
            "s=conversation\r\n"
            "c=IN IP4 %s\r\n"
            "t=0 0\r\n"
            // "m=audio %d RTP/AVP 0 8\r\n"
            "m=audio %d RTP/AVP %d\r\n"
            // a=rtpmap:96 L16/8000
            // "a=rtpmap:0 PCMU/8000\r\n"
            // "a=rtpmap:8 PCMA/8000\r\n"
            "a=rtpmap:%d %s/8000\r\n"
            "a=rtpmap:%d telephone-event/8000\r\n"
            "a=fmtp:%d 0-16\r\n"
            "a=sendrecv\r\n"
            "%s"
            // "m=video %d RTP/AVP %d\r\n"
            // "a=rtpmap:%d H264/90000\r\n"
            // "a=fmtp:%d packetization-mode=1\r\n"
            // // "a=ssrc:16909060\r\n"
            // // profile-level-id=428015
            // "a=sendrecv\r\n"
            // "a=ptime:40 \r\n"
            , localip, localip, 
                m_audio_rtp_local_port,
                audioPayloadType, audioPayloadType, audioStr.c_str(),
                telePayloadType, telePayloadType, video_tmp);
                // m_video_rtp_local_port, 
                // video_rtp_type, video_rtp_type, video_rtp_type);

    osip_message_set_body (answer, tmp, strlen (tmp));
    osip_message_set_content_type (answer, "application/sdp");

    sip_prepend_route(answer);
    eXosip_call_send_answer(m_context_eXosip, tid, 200, answer);
    m_is_calling = true;
    
    // osip_from_t * from = osip_message_get_from(event->request);
    // m_remote_user = from->url->username;
    // printf("!!!!!! remote  call user %s\n",from->url->username);
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

std::string uint8ArrayToHexString(const uint8_t* array, size_t size) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<int>(array[i]);
    }

    return oss.str();
}

std::vector<uint8_t> hexStringToByteArray(const std::string& hex, int len) 
{
    std::vector<uint8_t> byteArray;
	int bytelen = len / 2;
	std::string strByte;
	unsigned int n;
	for (int i = 0; i < bytelen; i++) 
	{
		strByte = hex.substr(i * 2, 2);
		sscanf(strByte.c_str(),"%x",&n);
		// bytes[i] = n;
        byteArray.push_back(n);
	}
    return byteArray;
}


void SipSession::sip_prepend_route(osip_message_t *sip){
    if(!m_sip_proxy.empty()){
        _prepend_route(sip, m_sip_proxy.c_str());
    }
}

std::future<void> g_future;
void SipSession::sipRun(){
    SipProxy proxy;
    if(ConfigServer::GetInstance()->GetSipProxy(proxy)){
        m_sip_proxy = "sip:"+proxy.m_addr + ":" + to_string(proxy.m_port);
        eXosip_add_authentication_info(m_context_eXosip, proxy.m_username.c_str(), 
            proxy.m_username.c_str(), proxy.m_password.c_str(), NULL, NULL);
    }

    int rid = 0;    
    string from = "sip:" + m_user_name + "@" + m_sip_server_domain;
    string to = "sip:" + m_sip_server_domain;
    
    cout<<"prepare register from "<<from << " to "<< to<<endl;
    osip_message_t *reg = nullptr;
    
    uint16_t tt = 0;
    int open_mutex_channel = -1;
    int register_fail_count = 0;
    int answer_sleep = ConfigServer::GetInstance()->GetAnwserSleep();
    int reg_time = 30;
    // if(ConfigServer::GetInstance()->GetEnableSipTcp()){
    //     reg_time = 10;
    // }
    while(!m_exited){
        if(tt % reg_time == 0){
            // eXosip_lock(m_context_eXosip);
            if(rid > 0){
                eXosip_register_remove(m_context_eXosip, rid);
            }
            
            rid = eXosip_register_build_initial_register(m_context_eXosip, from.c_str(), to.c_str(), NULL, 180, &reg);
            if (rid < 0)
            {
                printf("eXosip_register_build_initial_register fail %d\n", rid);
            }else{
                sip_prepend_route(reg);
                eXosip_register_send_register(m_context_eXosip, rid, reg);
            }
            // eXosip_unlock(m_context_eXosip);
        }

        tt++;
        osip_message_t *answer = NULL;
        osip_body *body = nullptr;
        int ret = 0;
        eXosip_event_t *event = eXosip_event_wait(m_context_eXosip, 1, 0);
        if(event){
            eXosip_lock(m_context_eXosip);
            eXosip_automatic_action(m_context_eXosip);
            if(event->type != EXOSIP_REGISTRATION_SUCCESS && event->type != EXOSIP_REGISTRATION_FAILURE){
                cout<<"SIP EVENT "<<event->textinfo<<" type "<< event->type<<endl;
            }
            
            // cout<<"event did "<<event->did<<endl;
            int did = event->did;
            int tid = event->tid;
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
		        case EXOSIP_CALL_RINGING:
		            m_call_did = event->did;
		            break;
                case EXOSIP_CALL_INVITE:
                    cout<<"SIP EVENT "<<event->textinfo<<" type "<< event->type<<endl;
                    if(m_AudioStream_ptr->IsOpened() || m_is_calling){
                        eXosip_call_send_answer(m_context_eXosip, event->tid, 
                            SIP_BUSY_HERE, NULL);
                        m_CtrlProtocol_ptr->SendCallResult(DB_Result_Busy);
                    }else{
                        m_is_calling = true;
                        osip_from_t * from = osip_message_get_from(event->request);
                        if(from && from->url){
                            char *from_str = nullptr;
                            osip_uri_to_str(from->url, &from_str);
                            m_call_to = from_str;
                            if(from_str) free(from_str);
                        }

                        
			            m_call_did = event->did;
                        if(answer_sleep > 0){
                            eXosip_call_send_answer(m_context_eXosip, event->tid, 
                                SIP_RINGING, NULL);
                            
                            g_future = std::async(std::launch::async, [this, did, tid,answer_sleep](){
                                // this_thread::sleep_for(chrono::seconds(answer_sleep));
                                this->m_CtrlProtocol_ptr->SendCallResult(DB_Result_Calling);
                                unique_lock<std::mutex> lock(this->m_call_mutex);
                                this->m_call_condition.wait_for(lock, chrono::seconds(answer_sleep));
                                if(m_is_calling){
                                    eXosip_lock(this->m_context_eXosip);
                                    this->outCallAnswer(did, tid);
                                    eXosip_unlock(this->m_context_eXosip);
                                }
                            });
                            break;
                        }
                        m_CtrlProtocol_ptr->SendCallResult(DB_Result_Calling);
                        outCallAnswer(did, tid);
                    }
                    break;
                case EXOSIP_CALL_REINVITE:
                    // AudioStream::GetInstance()->Close();
                    // VideoStream::GetInstance()->Close();
                    m_CtrlProtocol_ptr->SendCallResult(DB_Result_Calling);
                    outCallAnswer(did, tid);
                    break;
                case EXOSIP_CALL_REQUESTFAILURE:
                    // m_CtrlProtocol_ptr->SendCallResult(DB_Result_Failed);
                    if(event->did == m_call_did || m_call_did == 0){
                        m_CtrlProtocol_ptr->SendCallResult(DB_Result_HangUp);
                    }
                    TerminateCalling();
                    break;
                case EXOSIP_CALL_MESSAGE_NEW:
                    osip_message_get_body(event->request, 0, &body);
                    if(body){
                        if(body->length > 0){
                            m_CtrlProtocol_ptr->SendTunnelData((uint8_t*)body->body, body->length);
                        }
                        
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

                        ret = eXosip_call_build_answer(m_context_eXosip, event->tid, 200, &answer);
                        if(ret == 0){
                            eXosip_call_send_answer(m_context_eXosip, event->tid, 200, answer);
                        }else{
                            printf("eXosip_call_build_answer error %s\n", osip_strerror(ret));
                        }
                    }

                    break;
                case EXOSIP_CALL_PROCEEDING:
                    break;
                case EXOSIP_CALL_ANSWERED:
                    if(m_is_calling){
                        callAckAnswered(event);
                    }
                    break;
                
                case EXOSIP_CALL_RELEASED:
                    break;
                case EXOSIP_CALL_CLOSED:
                    if(event->did == m_call_did){
                        m_AudioStream_ptr->Close();
                        m_VideoStream_ptr->Close();
			            m_call_did = 0;
                        if(m_is_calling){
                            m_CtrlProtocol_ptr->SendCallResult(DB_Result_HangUp);
                        }
                        m_is_calling = false;
                    }
                    break;
                
                case EXOSIP_CALL_GLOBALFAILURE:
                    m_AudioStream_ptr->Close();
                    m_VideoStream_ptr->Close();
                    m_call_did = 0;
                    if(m_is_calling){
                        m_CtrlProtocol_ptr->SendCallResult(DB_Result_HangUp);
                    }
                    m_is_calling = false;     
                    break;
                case EXOSIP_CALL_CANCELLED:
                    // ret = eXosip_call_build_answer(m_context_eXosip, event->tid, 200, &answer);
                    // if(ret == 0){
                    //     eXosip_call_send_answer(m_context_eXosip, event->tid, 200, answer);
                    // }
                    if(event->did == m_call_did || m_call_did == 0){
                        m_CtrlProtocol_ptr->SendCallResult(DB_Result_HangUp);
                    }
                    TerminateCalling();
                    break;
                case EXOSIP_MESSAGE_REQUESTFAILURE:
                    eXosip_default_action(m_context_eXosip, event);
                    break;

                case EXOSIP_MESSAGE_NEW:
                    ret = eXosip_message_build_answer(m_context_eXosip, event->tid, 200, &answer);
                    if (ret == 0) {
                        eXosip_message_send_answer(m_context_eXosip, event->tid, 200, answer);
                    }

                    osip_message_get_body(event->request, 0, &body);
                    if(body){
                        if(body->length > 0){
                            char *start_prefix = strstr(body->body, "hex_data=");
                            if(!start_prefix){
                                break;
                            }
                            char *start_hex = start_prefix + strlen("hex_data=");

                            printf("recv MESSAGE %s\n", start_hex);
                            // m_CtrlProtocol_ptr->SendTunnelData((uint8_t *)body->body, body->length);
                            int len = strlen(start_hex);
                            if(body->body[body->length-2] == '\r' && 
                                body->body[body->length-1] == '\n'){
                                len -= 2;
                            }
                            if(len > 0){
                                auto data_v = hexStringToByteArray(start_hex, len);
                                if(data_v.size() > 0)
                                    for(int i = 0; i< data_v.size(); i++){
                                        printf("%x ", data_v[i]);
                                    }
                                    printf("\n");
                                    m_CtrlProtocol_ptr->SendTunnelData(data_v.data(), data_v.size());
                            }
                        }
                    }

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

void SipSession::SendTunnel(uint8_t *data, int size){
    if(nullptr == data){
        return;
    }

    string data_str = uint8ArrayToHexString((const uint8_t*)data, size);
    for(int i = 0; i < size; i++){
        printf(" %x ", data[i]);
    }
    printf("\n");
    printf("###### SendTunnel data %s\n", data_str.c_str());

    eXosip_lock(m_context_eXosip);
    osip_message_t *request = nullptr;

    string from = "sip:" + m_user_name + "@" + m_sip_server_domain;
    if(!m_AudioStream_ptr->IsOpened()){
        printf(" must send in calling \n");
        eXosip_unlock(m_context_eXosip);
        return;
    }

    string to = m_call_to;
    int i = eXosip_message_build_request(m_context_eXosip, &request, "MESSAGE", 
        to.c_str(), from.c_str(), NULL);
    if(i != 0){
        printf("eXosip_message_build_request fail  m_call_did %d, ret %s\n", m_call_did, osip_strerror(i));
        eXosip_unlock(m_context_eXosip);
        return;
    }
    
    if(request){
        //application/octet-stream  application/dtmf-relay text/plain
        osip_message_set_content_type(request, "text/plain");
        string send_data = "hex_data=" + data_str;
        osip_message_set_body(request, send_data.c_str(), send_data.size());
        sip_prepend_route(request);
        eXosip_message_send_request(m_context_eXosip, request);
    }
    
    printf("eXosip_message_send_request  success\n");
    eXosip_unlock(m_context_eXosip);

}



bool SipSession::CallOutgoing(const string &toUser, const string  dst_addr){
    if(m_is_calling || m_AudioStream_ptr->IsOpened()){
        cerr<<"is calling ,please close call"<<endl;
        m_CtrlProtocol_ptr->SendCallResult(DB_Result_Talking);
        return false;
    }

    string to;
    string from;
    
    if(dst_addr.empty()){
        to = "<sip:" + toUser + "@"+m_sip_server_domain+">";
        from = "<sip:" + m_user_name + "@"+m_sip_server_domain+">";
    }else{
        to = "<sip:" + toUser + "@"+dst_addr+">";
        from = "<sip:" + m_user_name + "@"+dst_addr+">";
    }

    if(!m_exited){
        eXosip_lock(m_context_eXosip);
        osip_message_t *invite;
        int cid;
        
        int i = eXosip_call_build_initial_invite (m_context_eXosip, &invite, to.c_str(),
                                            from.c_str(),
                                            NULL, // optional route header
                                            "This is a call for a conversation");
        if (i != 0)
        {
            printf("eXosip_call_build_initial_invite fail\n");
            m_CtrlProtocol_ptr->SendCallResult(DB_Result_Failed);
            eXosip_unlock(m_context_eXosip);
            return false;
        }

        char tmp[4096];
        char localip[128];

        if(m_local_ip == "0.0.0.0"){
            eXosip_guess_localip(m_context_eXosip, AF_INET, localip, 128);
        }else{
            strcpy(localip, m_local_ip.c_str());
        }


        auto local_codecs = ConfigServer::GetInstance()->GetAudioCodec();
        string audio_rtpmap_list;
        for(auto i:local_codecs){
            if(i == PCM){
                audio_rtpmap_list += "a=rtpmap:98 L16/8000\r\n";
            }else if(i == G711U){
                audio_rtpmap_list += "a=rtpmap:0 PCMU/8000\r\n";
            }else if(i == G711A){
                audio_rtpmap_list += "a=rtpmap:8 PCMA/8000\r\n";
            }else if(i == OPUS){
                audio_rtpmap_list += "a=rtpmap:97 OPUS/48000\r\n";
            }
        }

        snprintf (tmp, 4096,
                "v=0\r\n"
                "o=jack 0 0 IN IP4 %s\r\n"
                "s=conversation\r\n"
                "c=IN IP4 %s\r\n"
                "t=0 0\r\n"
                "m=audio %d RTP/AVP 0 8\r\n"
                // a=rtpmap:96 L16/8000
                // "a=rtpmap:0 PCMU/8000\r\n"
                // "a=rtpmap:8 PCMA/8000\r\n"
                "%s"
                "a=rtpmap:101 telephone-event/8000\r\n"
                "a=fmtp:101 0-16\r\n"
                "a=sendrecv\r\n"

                "m=video %d RTP/AVP 99 \r\n"
                "a=rtpmap:99 H264/90000 \r\n"
                "a=fmtp:99 packetization-mode=1\r\n"
                // "a=sendonly \r\n"
                // "a=ptime:40 \r\n"
                , localip, localip, m_audio_rtp_local_port, 
                audio_rtpmap_list.c_str(),m_video_rtp_local_port);
                // "a=rtpmap:101 telephone-event/8000\r\n"
                // "a=fmtp:101 0-11\r\n", localip, localip, 3000);
        osip_message_set_body (invite, tmp, strlen(tmp));
        osip_message_set_content_type (invite, "application/sdp");

        // eXosip_lock(m_context_eXosip);
        sip_prepend_route(invite);
        cid = eXosip_call_send_initial_invite (m_context_eXosip, invite);
        eXosip_unlock(m_context_eXosip);
        if(cid < 0){
            printf("eXosip_call_send_initial_invite fail\n");
            m_CtrlProtocol_ptr->SendCallResult(DB_Result_Failed);
            return false;
        }
	    m_call_cid = cid;
    }

    printf("CallOutgoing ok\n");
    m_CtrlProtocol_ptr->SendCallResult(DB_Result_Calling);
    
    m_call_to = to;
    m_is_calling = true;
    return true;
}
int SipSession::TerminateCalling(){
    cout<<"!!!!!! TerminateCalling"<<endl;
    this_thread::sleep_for(chrono::milliseconds(20));

    // eXosip_lock(m_context_eXosip);
    eXosip_call_terminate(m_context_eXosip, m_call_cid, m_call_did);
    // eXosip_unlock(m_context_eXosip);
    m_AudioStream_ptr->Close();
    m_VideoStream_ptr->Close();
    m_is_calling = false;
    m_call_condition.notify_one();
    return 0;
}

bool SipSession::GetRegStatus(){
    return m_is_registed;
}
void SipSession::openMutexCtl(int channel){
    m_CtrlProtocol_ptr->OpenMutex(channel);
}

void SipSession::Answer_call(){
    m_call_condition.notify_one();
}