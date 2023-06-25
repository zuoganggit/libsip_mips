#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory.h>

#include "ctrlProtocol.h"
#include "T21_Def.h"


CtrlProtocol::CtrlProtocol(){
    m_t21_socket = -1;
    m_exited = false;
}

CtrlProtocol::~CtrlProtocol(){
    m_exited = true;
    m_run_future.wait();
    closeUdpSocket();
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
    t21_data.CommandID = DB_CMD_DOControl_Request;
    t21_data.Version = 0x01;
    t21_data.CommandFlag = 0x12;
    t21_data.TotalSegment = 0x01;
    t21_data.SubSegment = 0x01;
    t21_data.SegmentFlag = 0x01;
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_OpenMutex_Req_Payload payload = {(uint32_t)channel};
    int buffer_size = sizeof(T21_OpenMutex_Req_Payload) + sizeof(T21_Data);

    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_OpenMutex_Req_Payload));
    if (0 == send(m_t21_socket, buffer, buffer_size, 0)){
        cout<<"OpenMutex request fail"<<endl;
    }else{
        cout<<"OpenMutex request "<<endl;
    }
}


void CtrlProtocol::CallNotify(int acount_index){
    
}

void CtrlProtocol::OpenAudioChannel(){
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = DB_CMD_Play_Request;
    t21_data.Version = 0x01;
    t21_data.CommandFlag = 0x12;
    t21_data.TotalSegment = 0x01;
    t21_data.SubSegment = 0x01;
    t21_data.SegmentFlag = 0x01;
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Media_Payload payload = {1, DB_MediaMode_AUDIO_Capture};
    int buffer_size = sizeof(T21_Ctrl_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Media_Payload));
    if (0 == send(m_t21_socket, buffer, buffer_size, 0)){
        cout<<"OpenAudioChannel request fail"<<endl;
    }else{
        cout<<"OpenAudioChannel request "<<endl;
    }
}

void CtrlProtocol::OpenVideoChannel(){
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = DB_CMD_Play_Request;
    t21_data.Version = 0x01;
    t21_data.CommandFlag = 0x12;
    t21_data.TotalSegment = 0x01;
    t21_data.SubSegment = 0x01;
    t21_data.SegmentFlag = 0x01;
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Media_Payload payload = {1, DB_MediaMode_VIDEO};
    int buffer_size = sizeof(T21_Ctrl_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Media_Payload));
    if (0 == send(m_t21_socket, buffer, buffer_size, 0)){
        cout<<"OpenVideoChannel request fail"<<endl;
    }else{
        cout<<"OpenVideoChannel request "<<endl;
    }
}

void CtrlProtocol::CloseAudioChannel(){
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = DB_CMD_Stop_Request;
    t21_data.Version = 0x01;
    t21_data.CommandFlag = 0x12;
    t21_data.TotalSegment = 0x01;
    t21_data.SubSegment = 0x01;
    t21_data.SegmentFlag = 0x01;
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Media_Payload payload = {1, DB_MediaMode_AUDIO_Capture};
    int buffer_size = sizeof(T21_Ctrl_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Media_Payload));
    if (0 == send(m_t21_socket, buffer, buffer_size, 0)){
        cout<<"CloseAudioChannel request fail"<<endl;
    }else{
        cout<<"CloseAudioChannel request "<<endl;
    }
}

void CtrlProtocol::CloseVideoChannel(){
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = DB_CMD_Stop_Request;
    t21_data.Version = 0x01;
    t21_data.CommandFlag = 0x12;
    t21_data.TotalSegment = 0x01;
    t21_data.SubSegment = 0x01;
    t21_data.SegmentFlag = 0x01;
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Ctrl_Media_Payload payload = {1, DB_MediaMode_VIDEO};
    int buffer_size = sizeof(T21_Ctrl_Media_Payload) + sizeof(T21_Data);
    uint8_t * buffer = new uint8_t[buffer_size];
    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &payload, sizeof(T21_Ctrl_Media_Payload));
    if (0 == send(m_t21_socket, buffer, buffer_size, 0)){
        cout<<"CloseVideoChannel request fail"<<endl;
    }else{
        cout<<"CloseVideoChannel request "<<endl;
    }
}


bool CtrlProtocol::openUdpSocket(){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置本地地址
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(T21_PORT);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 绑定本地地址
    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "Failed to bind socket to local address" << std::endl;
        return false;
    }

    m_t21_socket = sock;

    // 设置阻塞超时时间
    timeval timeout{};
    timeout.tv_sec = 1;  // 设置超时时间为1秒
    timeout.tv_usec = 0;

    if (setsockopt(m_t21_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout)) < 0) {
        std::cerr << "Failed to set receive timeout" << std::endl;
    }

    sockaddr_in destinationAddr{};
    destinationAddr.sin_family = AF_INET;
    destinationAddr.sin_port = htons(T21_PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &(destinationAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid destination IP address" << std::endl;
    }else{
        m_destinationAddr = destinationAddr;
    }

    return true;
}

void CtrlProtocol::closeUdpSocket(){
    if(m_t21_socket > 0){
        close(m_t21_socket);
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
              data->GroupCode, data->CommandID, data->Version, data->CommandFlag,
              data->TotalSegment, data->SubSegment, data->SegmentFlag,
              data->Reserved1, data->Reserved2, data->Payload);
        }

        // printf("continue   recv  t21 data bytes %d\n",bytes);
    }
}