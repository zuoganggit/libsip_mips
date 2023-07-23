#include <iostream>
#include <memory.h>
#include "audioStream.h"
#include "T21_Def.h"
extern "C"{
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
}

AudioStream::AudioStream(int t21port){
    m_opening = false;
    m_t21_port = t21port;
    m_frame_callback = nullptr;
    m_inEncodeType = ET_G711U;
    Init();
}
    
AudioStream::~AudioStream(){
    cout<<"~AudioStream()"<<endl;

    Close();
    if(m_socket > 0){
        close(m_socket);
    }
}

shared_ptr<AudioStream> AudioStream::GetInstance(int t21port){
    static auto stream_ptr = make_shared<AudioStream>(t21port);
    return stream_ptr;
}


bool AudioStream::Init(){
    //start udp t21port service
    //recv audio pcm stream
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置本地地址
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(AUDIO_LOCAL_PORT);
    // localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (inet_pton(AF_INET, "127.0.0.1", &(localAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid  IP address" << std::endl;
    }

    // 绑定本地地址
    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "AudioStream Failed to bind socket to local address port " << AUDIO_LOCAL_PORT<<endl;
        return false;
    }

    m_socket = sock;

    // 设置阻塞超时时间
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout)) < 0) {
        std::cerr << "Failed to set receive timeout" << std::endl;
    }

    m_destinationAddr.sin_family = AF_INET;
    m_destinationAddr.sin_port = htons(AUDIO_REMOTE_PORT);
    if (inet_pton(AF_INET, REMOTE_ADDR, &(m_destinationAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid destination IP address" << std::endl;
    }

    return true;
}

void AudioStream::Open(FrameCallback callback, AudioEncodeType_e in_type, AudioEncodeType_e out_type){
    if(!m_opening){
        m_opening = true;
        //send open Audio Stream to T21
        CtrlProtocol::GetInstance()->OpenAudioOutChannel(out_type);
        CtrlProtocol::GetInstance()->OpenAudioInChannel(in_type);
        m_inEncodeType = in_type;
    }
    else{
        return;
        // Close();
    }

    m_frame_callback = callback;
    m_stream_run_future = std::async(std::launch::async, [this](){
        this->run();
    });
}

// void AudioStream::Open(FrameCallback callback){
//     if(!m_opening){
//         m_opening = true;
//         //send open Audio Stream to T21
//         CtrlProtocol::GetInstance()->OpenAudioChannel();
//     }
//     else{
//         return;
//         // Close();
//     }

//     m_frame_callback = callback;
//     m_stream_run_future = std::async(std::launch::async, [this](){
//         this->run();
//     });
// }

bool AudioStream::IsOpened(){
    return m_opening;
}
void AudioStream::Close(){
    if(!m_opening) return;

    //send close Audio Stream to T21
    CtrlProtocol::GetInstance()->CloseAudioChannel();
    m_opening = false;
    m_stream_run_future.wait();
    m_frame_callback = nullptr;
}
void AudioStream::WriteAudioFrame(uint8_t* data, int dataSize){
    // cout<<"WriteAudioFrame dataSize "<<dataSize<<endl;
    T21_Data t21_data = {0};
    t21_data.GroupCode = 0xDB;
    t21_data.CommandID = htons(DB_CMD_Send_Media_Request_EX);
    t21_data.Version = 0x01;
    t21_data.CommandFlag = htonl(0x12);
    t21_data.TotalSegment = htons(0x01);
    t21_data.SubSegment = htons(0x01);
    t21_data.SegmentFlag = htons(0x01);
    t21_data.Reserved1 = 0;
    t21_data.Reserved2 = 0;

    T21_Send_Media_ReqEx_Payload t21payload = {htonl(1), htonl(1),
        0, m_inEncodeType, htonl(dataSize)};
    
    int buffer_size = sizeof(T21_Data) + sizeof(T21_Send_Media_ReqEx_Payload) + dataSize;
    uint8_t * buffer = new uint8_t[buffer_size];

    memcpy(buffer, &t21_data, sizeof(T21_Data));
    memcpy(buffer+sizeof(T21_Data), &t21payload, sizeof(T21_Send_Media_ReqEx_Payload));
    memcpy(buffer+sizeof(T21_Data)+sizeof(T21_Send_Media_ReqEx_Payload), data, dataSize);

    sendto(m_socket, buffer, buffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
    delete[] buffer;
}

#include <fstream>
#include <thread>

ifstream *file = nullptr;
void testSendPcmFile(FrameCallback callback){
    if(file == nullptr){
        file = new ifstream("output.pcm", std::ios::binary);
    }

    char pcmData[320] = {0};
    file->read(pcmData, sizeof(pcmData));
    if (file->eof()) {
        cout<<"read pcm file eof"<<endl;
        file->close();
        delete file;
        file = new ifstream("output.pcm", std::ios::binary);
        return ;
    }
    //PackRTPPacket
    callback((uint8_t *)pcmData, sizeof(pcmData));
}

void AudioStream::run_test(){
    cout<<"AudioStream::run  start"<<endl;
    while(m_opening){
        if(m_frame_callback != nullptr){
            testSendPcmFile(m_frame_callback);
        }
        this_thread::sleep_for(chrono::milliseconds(40));
    }

    cout<<"AudioStream::run  end"<<endl;
    if(file){
        file->close();
        delete file;
        file = nullptr;
    }
}


void AudioStream::run(){
    int buffer_size = 1550;
    uint8_t buffer[buffer_size] = {0};
    struct sockaddr_in remoteAddr{};
    socklen_t addrLen = sizeof(remoteAddr);
    bool send_n = false;
    while(m_opening){
        ssize_t bytesRead = recvfrom(m_socket, buffer, buffer_size, 0, (struct sockaddr*)&remoteAddr, &addrLen);
        T21_Data* t_data = (T21_Data*)buffer;
        // printf("bytesRead  %d\n", bytesRead);
        if(bytesRead > 0 && bytesRead > sizeof(T21_Data)){
            int commonId = ntohs(t_data->CommandID);
            //printf("recv audio data commonId %x,  bytesRead %d\n", commonId, bytesRead);
            // printf("audio buffer ");
            // for(ssize_t i = 0; i < bytesRead; i++){
            //     printf(" %x , ", buffer[i]);
            // }
            // printf("\n\n");


            if(commonId == DB_CMD_Send_Media_Request_EX){
                T21_Send_Media_ReqEx_Payload *t21payload = (T21_Send_Media_ReqEx_Payload *)t_data->Payload;
                // printf("t21payload->m_medialength code %x, ntohl  %d\n", t21payload->m_medialength, ntohl(t21payload->m_medialength));
                if(bytesRead >= sizeof(T21_Data) + sizeof(T21_Send_Media_ReqEx_Payload)){
                    int data_len = bytesRead - sizeof(T21_Data) - sizeof(T21_Send_Media_ReqEx_Payload);
                    // printf("bytesRead %d, T21_Data size %d, T21_Send_Media_ReqEx_Payload %d\n", bytesRead, 
                        // sizeof(T21_Data), sizeof(T21_Send_Media_ReqEx_Payload));
                    m_frame_callback(t21payload->m_mediadata, data_len);
                }
            }else if(commonId == DB_CMD_Send_Media_Request){
                T21_Send_Media_Req_Payload *t21payload = (T21_Send_Media_Req_Payload *)t_data->Payload;
                if(bytesRead >= sizeof(T21_Data) + sizeof(T21_Send_Media_Req_Payload)){
                    int data_len = bytesRead - sizeof(T21_Data) - sizeof(T21_Send_Media_Req_Payload);
                    m_frame_callback(t21payload->m_mediadata, data_len);
                }
            }else if(commonId == DB_CMD_Send_Media_Request_EX2){
                T21_Send_Media_ReqEx2_Payload *t21payload = (T21_Send_Media_ReqEx2_Payload *)t_data->Payload;
                if(bytesRead >= sizeof(T21_Data) + sizeof(T21_Send_Media_ReqEx2_Payload)){
                    int data_len = bytesRead - sizeof(T21_Data) - sizeof(T21_Send_Media_ReqEx2_Payload);
                    m_frame_callback(t21payload->m_mediadata, data_len);
                }

                T21_Data t21_data = {0};
                t21_data.GroupCode = 0xDB;
                t21_data.CommandID = htons(DB_CMD_Send_Media_Result);
                t21_data.Version = 0x01;
                t21_data.CommandFlag = htonl(0x12);
                t21_data.TotalSegment = htons(0x01);
                t21_data.SubSegment = htons(0x01);
                t21_data.SegmentFlag = htons(0x01);
                t21_data.Reserved1 = 0;
                t21_data.Reserved2 = 0;
                T21_Send_Media_Res_Payload res_payload = {htonl(t21payload->m_sequence)};

                int pbuffer_size = sizeof(T21_Data) + sizeof(T21_Send_Media_Res_Payload);
                uint8_t * pbuffer = new uint8_t[pbuffer_size];
                memcpy(pbuffer, &t21_data, sizeof(T21_Data));
                memcpy(pbuffer+sizeof(T21_Data), &res_payload, sizeof(T21_Send_Media_Res_Payload));
                sendto(m_socket, pbuffer, pbuffer_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
                delete[] pbuffer;
            }
        }else{
            if(!send_n){
                uint8_t data[10] = {0};
                m_frame_callback(data, 10);
                send_n = true;
            }
        }

    }
}