#include "videoStream.h"
#include "T21_Def.h"
extern "C"{
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
}
#include <vector>
#include <memory.h>

VideoStream::VideoStream(int t21port){
    m_opening = false;
    m_frame_callback = nullptr;
    m_socket = 0;
    Init();
}
VideoStream::~VideoStream(){
    cout<<"~VideoStream()"<<endl;
    Close();
    if(m_socket > 0){
        close(m_socket);
    }
}

shared_ptr<VideoStream> VideoStream::GetInstance(int t21port){
    static auto viedeo_ptr = make_shared<VideoStream>(t21port);
    return viedeo_ptr;
}
bool VideoStream::Init(){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置本地地址
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(VIDEO_LOCAL_PORT);
    // localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (inet_pton(AF_INET, "127.0.0.1", &(localAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid  IP address" << std::endl;
    }
    // 绑定本地地址
    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "AudioStream Failed to bind socket to local address port " << VIDEO_LOCAL_PORT<<endl;
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

    return true;
}
void VideoStream::Open(FrameCallback callback){
    if(!m_opening){
        m_opening = true;
        //send open Video Stream to T21
        CtrlProtocol::GetInstance()->OpenVideoChannel();
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
bool VideoStream::IsOpened(){

    return m_opening;
}
void VideoStream::Close(){
    if(!m_opening) return;

    //send close Audio Stream to T21
    CtrlProtocol::GetInstance()->CloseVideoChannel();
    m_opening = false;
    m_stream_run_future.wait();
    m_frame_callback = nullptr;
}


// #include <fstream>
// #include <thread>
// #include <iostream>
// #include <vector>
// #include <memory.h>

// ifstream *inputFile = nullptr;
// std::vector<uint8_t> nalBuffer;
// std::vector<uint8_t> frameBuffer;
// bool getNextFrame(uint8_t*& naluData, int& naluSize){

//     return true;
// }

// bool getNextNalu(uint8_t*& naluData, int& naluSize) {
//     naluData = nullptr;
//     naluSize = 0;

//     if (!inputFile) {
//         std::cerr << "File not opened." << std::endl;
//         return false;
//     }

//     // 清空上一个 NALU 的数据
//     nalBuffer.clear();
//     // 读取文件直到找到 NALU 起始码
//     while (true) {
//         uint8_t byte;
//         if (!inputFile->read(reinterpret_cast<char*>(&byte), 1)) {
//             // 文件读取完毕
//             if (nalBuffer.empty()) {
//                 // 已经读取完所有 NALU 数据
//                 return false;
//             } else {
//                 // 最后一个 NALU 数据
//                 naluData = nalBuffer.data();
//                 naluSize = nalBuffer.size();
//                 return true;
//             }
//         }

//         nalBuffer.push_back(byte);
//         if (nalBuffer.size() >= 4) {
//             // 判断是否为 NALU 起始码
//             if ((nalBuffer[nalBuffer.size() - 4] == 0x00 && nalBuffer[nalBuffer.size() - 3] == 0x00 &&
//                  nalBuffer[nalBuffer.size() - 2] == 0x00 && nalBuffer[nalBuffer.size() - 1] == 0x01) ||
//                 (nalBuffer[nalBuffer.size() - 3] == 0x00 && nalBuffer[nalBuffer.size() - 2] == 0x00 &&
//                  nalBuffer[nalBuffer.size() - 1] == 0x01)) {

//                 // 找到 NALU 起始码
//                 naluData = nalBuffer.data();
//                 if(nalBuffer[nalBuffer.size() - 4] == 0x00){
//                     naluSize = nalBuffer.size() - 4;
//                 }else{
//                     naluSize = nalBuffer.size() - 3;
//                 }
                
//                 return true;
//             }
//         }
//     }
// }

// void testSendH264File(FrameCallback callback){
//     if(inputFile == nullptr){
//         inputFile = new ifstream("video11.264", std::ios::binary);
//     }

//     uint8_t* naluData = nullptr;
//     int naluSize;
//     if(getNextNalu(naluData, naluSize)){
//         // cout<<"h264 frame size "<<naluSize<<endl;
//         if(naluSize > 0){
//             // printf("h264 frame %x, %x, %x \n", naluData[0],naluData[1],naluData[2]);
//             uint8_t nal_unit_type = naluData[0] & 0x1F;
//             // printf("!!!!!! nal_unit_type %d, size %d \n", nal_unit_type, naluSize);

//             if(nal_unit_type == 7 || nal_unit_type == 1){
//                 if(frameBuffer.size() > 0){
//                     callback((uint8_t *)frameBuffer.data(), frameBuffer.size());
//                     frameBuffer.clear();
//                     this_thread::sleep_for(chrono::milliseconds(40));
//                 }
//             }

//             frameBuffer.push_back(0);
//             frameBuffer.push_back(0);
//             frameBuffer.push_back(0);
//             frameBuffer.push_back(0x01);
//             for(int i = 0; i < naluSize; i++){
//                 frameBuffer.push_back(naluData[i]);
//             }
//         }
//     }else{
//         cout<<"read video11.264 file end"<<endl;
//         inputFile->close();
//         delete inputFile;
//         inputFile = new ifstream("video11.264", std::ios::binary);
//     }
    
// }


void VideoStream::run_new(){
    // cout<<"VideoStream::run  start"<<endl;
    // while(m_opening){
    //     if(m_frame_callback != nullptr){
    //         testSendH264File(m_frame_callback);
    //     }
    //     // this_thread::sleep_for(chrono::milliseconds(40));
    // }

    // cout<<"VideoStream::run  end"<<endl;
    // if(inputFile){
    //     inputFile->close();
    //     delete inputFile;
    //     inputFile = nullptr;
    // }
}


void VideoStream::run(){
    //接收视频帧数据，并回调到上层
    int buffer_size = 1550;
    uint8_t buffer[buffer_size] = {0};
    struct sockaddr_in remoteAddr{};
    socklen_t addrLen = sizeof(remoteAddr);
    vector<uint8_t> frame_buffer;
    int max_frame_size = 2*1024*1024;

    auto frame_handle = [=](vector<uint8_t>& frame_buffer, uint8_t* data, int size, int seq){
        if(seq == 1){
            if(frame_buffer.size() > 0 && frame_buffer.size() <= max_frame_size){
                this->m_frame_callback(frame_buffer.data(), frame_buffer.size());
            }
            frame_buffer.clear();
        }else{
            if(frame_buffer.size() > max_frame_size){
                cout << "frame_size too big "<<frame_buffer.size()<<endl;
                return;
            }
        }
        for(int i = 0; i < size; i++){
            frame_buffer.push_back(data[i]);
        }
    };
    while(m_opening){
        ssize_t bytesRead = recvfrom(m_socket, buffer, buffer_size, 0, (struct sockaddr*)&remoteAddr, &addrLen);
        T21_Data* t_data = (T21_Data*)buffer;

        if(bytesRead > 0 && bytesRead > sizeof(T21_Data)){
            int commonId = ntohs(t_data->CommandID);
            if(commonId == DB_CMD_Send_Media_Request_EX){
                // printf("recv t21 video data\n");
                T21_Send_Media_ReqEx_Payload *t21payload = (T21_Send_Media_ReqEx_Payload *)t_data->Payload;
                if(bytesRead >= sizeof(T21_Data) + sizeof(T21_Send_Media_ReqEx_Payload)){
                   int data_len = bytesRead - sizeof(T21_Data) - sizeof(T21_Send_Media_ReqEx_Payload);
                   
                   frame_handle(frame_buffer, t21payload->m_mediadata, data_len, ntohl(t21payload->m_sequence));
                }
            }else if(commonId == DB_CMD_Send_Media_Request){
                T21_Send_Media_Req_Payload *t21payload = (T21_Send_Media_Req_Payload *)t_data->Payload;
                if(bytesRead >= sizeof(T21_Data) + sizeof(T21_Send_Media_Req_Payload)){
                   int data_len = bytesRead - sizeof(T21_Data) - sizeof(T21_Send_Media_Req_Payload);
                   frame_handle(frame_buffer, t21payload->m_mediadata, data_len, ntohl(t21payload->m_sequence));
                }
            }else if(commonId == DB_CMD_Send_Media_Request_EX2){
                T21_Send_Media_ReqEx2_Payload *t21payload = (T21_Send_Media_ReqEx2_Payload *)t_data->Payload;
                if(bytesRead >= sizeof(T21_Data) + sizeof(T21_Send_Media_ReqEx2_Payload)){
                    int data_len = bytesRead - sizeof(T21_Data) - sizeof(T21_Send_Media_ReqEx2_Payload);
                   frame_handle(frame_buffer, t21payload->m_mediadata, data_len, ntohl(t21payload->m_sequence));
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
                send(m_socket, pbuffer, pbuffer_size, 0);
                delete[] pbuffer;
            }
        }
    }
    frame_buffer.clear();
}