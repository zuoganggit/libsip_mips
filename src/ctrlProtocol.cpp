#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory.h>

#include "configServer.h"
#include "ctrlProtocol.h"
#include "sipSession.h"

CtrlProtocol::CtrlProtocol(){
    m_t21_socket = -1;
    m_exited = false;
}

CtrlProtocol::~CtrlProtocol(){
    cout<<"~CtrlProtocol()"<<endl;
    
    if(m_t21_socket > 0){
        m_exited = true;
        m_run_future.wait();
        closeUdpSocket();
    }
}

shared_ptr<CtrlProtocol> CtrlProtocol::GetInstance(){
    static shared_ptr<CtrlProtocol> ctrlPtr = nullptr;
    if(ctrlPtr == nullptr){
        ctrlPtr = make_shared<CtrlProtocol>();
        ctrlPtr->Init();
    }
    return ctrlPtr;
}

void CtrlProtocol::Init(){
    if(openUdpSocket()){
        m_run_future = std::async(std::launch::async, [this](){
            this->run();
        });
    }
}

void CtrlProtocol::OpenMutex(int channel){
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_DOControl_Request);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_OpenMutex_Req_Payload payload = {htonl(channel)};
    int buffer_size = sizeof(T21_OpenMutex_Req_Payload) + sizeof(T21_Data);

    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_OpenMutex_Req_Payload));
    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}


void CtrlProtocol::CallOutgoing(T21_Data *data){

    T21_Call_Req_Payload *payload = (T21_Call_Req_Payload *)data->Payload;
    if(payload != nullptr){
        int acount_index = payload->m_DIType;
        string dst_user;
        if(ConfigServer::GetInstance()->GetOutAccount(acount_index, dst_user)){
            if(SipSession::GetInstance()->CallOutgoing(dst_user)){
                // resPayload.m_result = htonl(DB_Result_Success);
                // SendCallResult(DB_Result_Calling);
                return;
            }
        }else{
            cerr<<"CallNotify GetOutAccount fail index "<<acount_index<<endl;
            SendCallResult(DB_Result_Failed);
            return;
        }
    }else{
        cerr<<"CallNotify not found T21_Call_Req_Payload "<<endl;
        SendCallResult(DB_Result_Failed);
        return;
    }
    // SendCallResult(DB_Result_Failed);
}


void CtrlProtocol::StopOutgoing(T21_Data *data){
    SipSession::GetInstance()->TerminateCalling();
}

void CtrlProtocol::SendCallResult(Result_e ret){
    printf("SendCallResult  %d\n", ret);
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Call_Result);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;
    int buffer_size = sizeof(T21_Call_Res_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    T21_Call_Res_Payload resPayload = {htonl(ret)};

    memcpy(buffer+sizeof(T21_Data), &resPayload, sizeof(T21_Call_Res_Payload));

    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}


void CtrlProtocol::SendRegStatus(T21_Data *data){
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Query_RegStatus_Result);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    int buffer_size = sizeof(T21_Call_Res_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));

    T21_Query_RegStatus_Res_Payload resPayload = {uint8_t(REGST_SUCCESS)};
    if(!SipSession::GetInstance()->GetRegStatus()){
        resPayload.m_regStatus = uint8_t(REGST_AUTH_FAILED);
    }
    printf("register %d\n",SipSession::GetInstance()->GetRegStatus());
    memcpy(buffer+sizeof(T21_Data), &resPayload, sizeof(T21_Call_Res_Payload));
    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}


void CtrlProtocol::TunnelDataHandle(T21_Data *data){
    T21_DB_CMD_Tunnel_Req_Payload *payload = (T21_DB_CMD_Tunnel_Req_Payload *)data->Payload;
    if(payload != nullptr){
        uint32_t datalen = ntohl(payload->m_datalen);
        //send sip info message
        printf("TunnelDataHandle datalen %d\n", datalen);
        if(datalen > 0){
            SipSession::GetInstance()->SendTunnel(payload->m_data, datalen);
        }
    }
}


void CtrlProtocol::SendTunnelData(uint8_t* data, int size){
    printf("SendTunnelData size %d\n", size);
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Tunnel_Request);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    int buffer_size = size + sizeof(T21_DB_CMD_Tunnel_Res_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));

    T21_DB_CMD_Tunnel_Res_Payload resPayload = {0};
    resPayload.m_datalen = htonl(uint32_t(size));
    uint8_t * dataBuffer = data;
    memcpy(buffer+sizeof(T21_Data), &resPayload, sizeof(T21_DB_CMD_Tunnel_Res_Payload));
    memcpy(buffer+sizeof(T21_Data)+sizeof(T21_DB_CMD_Tunnel_Res_Payload), data, size);

    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}


void CtrlProtocol::OpenAudioInChannel(AudioEncodeType_e audioType){
    printf("open channel DB_MediaMode_AUDIO_Play \n");
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Play_Request);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Open_Media_Payload payload = {(uint32_t)htonl(1), (uint8_t)audioType, (uint32_t)htonl(DB_MediaMode_AUDIO_Play)};
    int buffer_size = sizeof(T21_Ctrl_Open_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Open_Media_Payload));

    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}


void CtrlProtocol::OpenAudioOutChannel(AudioEncodeType_e audioType){
    printf("open channel DB_MediaMode_AUDIO_Capture \n");
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Play_Request);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;


    T21_Ctrl_Open_Media_Payload payload1 = {(uint32_t)htonl(1), (uint8_t)audioType, (uint32_t)htonl(DB_MediaMode_AUDIO_Capture)};
    int buffer_size = sizeof(T21_Ctrl_Open_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload1, sizeof(T21_Ctrl_Open_Media_Payload));
    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));

    delete[] buffer;

}


void CtrlProtocol::OpenVideoChannel(){
    printf("!!!!!!! OpenVideoChannel \n");
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Play_Request);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Open_Media_Payload payload = {htonl(1), (uint8_t)ET_G711U, htonl(DB_MediaMode_VIDEO)};
    int buffer_size = sizeof(T21_Ctrl_Open_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Open_Media_Payload));

    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}

void CtrlProtocol::CloseAudioChannel(){
    printf("!!!!!!! CloseAudioChannel \n");
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Stop_Request);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Close_Media_Payload payload = {htonl(1), htonl(DB_MediaMode_AUDIO_Capture)};
    int buffer_size = sizeof(T21_Ctrl_Close_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Close_Media_Payload));
    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    
    
    T21_Ctrl_Close_Media_Payload payload1 = {(uint32_t)htonl(1), (uint32_t)htonl(DB_MediaMode_AUDIO_Play)};
    int buffer_size1 = sizeof(T21_Ctrl_Close_Media_Payload) + sizeof(T21_Data);
    
    memcpy(buffer+sizeof(T21_Data), &payload1, sizeof(T21_Ctrl_Close_Media_Payload));
    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));

    delete[] buffer;
}

void CtrlProtocol::CloseVideoChannel(){
    printf("!!!!!!! CloseVideoChannel \n");
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Stop_Request);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Close_Media_Payload payload = {htonl(1), htonl(DB_MediaMode_VIDEO)};
    int buffer_size = sizeof(T21_Ctrl_Close_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Close_Media_Payload));
    sendto(m_t21_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}




bool CtrlProtocol::openUdpSocket(){
    m_t21_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_t21_socket < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置本地地址
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(LOCAL_PORT);
    // localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (inet_pton(AF_INET, "127.0.0.1", &(localAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid  IP address" << std::endl;
    }

    // 绑定本地地址
    if (bind(m_t21_socket, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "Failed to bind socket to local address" << std::endl;
        return false;
    }

    // 设置阻塞超时时间
    timeval timeout{};
    timeout.tv_sec = 1;  // 设置超时时间为1秒
    timeout.tv_usec = 0;

    if (setsockopt(m_t21_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout)) < 0) {
        std::cerr << "Failed to set receive timeout" << std::endl;
    }

    m_destinationAddr.sin_family = AF_INET;
    m_destinationAddr.sin_port = htons(T21_PORT);
    if (inet_pton(AF_INET, REMOTE_ADDR, &(m_destinationAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid destination IP address" << std::endl;
    }

    return true;
}

void CtrlProtocol::closeUdpSocket(){
    if(m_t21_socket > 0){
        close(m_t21_socket);
    }
}


void CtrlProtocol::t21CmdHandle(T21_Data *data){
    if (data == nullptr){
        return;
    }
    switch (ntohs(data->CommandID)){
    case DB_CMD_Play_Result:
        break;
    case DB_CMD_Stop_Result:
        break;
    case DB_CMD_Send_Media_Result:
        break;
    case DB_CMD_DOControl_Result:
        break;
    case DB_CMD_Send_Media_Request_EX2:
        break;
    case DB_CMD_Query_RegStatus_Request:
        SendRegStatus(data);
        break;
    case DB_CMD_Call_Request:
        CallOutgoing(data);
        break;
    case DB_CMD_HangUp_Request:
        StopOutgoing(data);
        break;
    case DB_CMD_Tunnel_Request:
        TunnelDataHandle(data);
        break;
    default:
        break;
    }
}

void CtrlProtocol::run(){
    char  buf[1500] = {0};
    socklen_t clientAddressLength = sizeof(m_destinationAddr);
    while(!m_exited){
        ssize_t bytes = recvfrom(m_t21_socket, (void *)buf, sizeof(buf), 0, 
        reinterpret_cast<sockaddr*>(&m_destinationAddr), &clientAddressLength);

        if(bytes > 0 && bytes >= sizeof(T21_Data)){
            T21_Data *data = (T21_Data *)buf;
            printf("recv t21 GroupCode %x, CommandID %x, \
              Version %x, CommandFlag %x, TotalSegment %x, \
              SubSegment %x, SegmentFlag %x, Reserved1 %x, \
              Reserved2 %x,  Payload %x \n",
              data->GroupCode, ntohs(data->CommandID), data->Version, ntohl(data->CommandFlag),
              ntohs(data->TotalSegment), ntohs(data->SubSegment), ntohs(data->SegmentFlag),
              ntohs(data->Reserved1), ntohl(data->Reserved2), data->Payload);
            
            t21CmdHandle(data);
        }
    }
}