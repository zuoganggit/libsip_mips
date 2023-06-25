#include "videoStream.h"
#include "T21_Def.h"


VideoStream::VideoStream(int t21port){
    m_opening = false;
    m_t21_port = t21port;
    m_frame_callback = nullptr;
}
VideoStream::~VideoStream(){
    Close();
}

shared_ptr<VideoStream> VideoStream::GetInstance(int t21port){
    static auto viedeo_ptr = make_shared<VideoStream>(t21port);
    return viedeo_ptr;
}
bool VideoStream::Init(){

    return true;
}
void VideoStream::Open(FrameCallback callback){
    if(!m_opening){
        m_opening = true;
        //send open Video Stream to T21
        CtrlProtocol::GetInstance()->OpenVideoChannel();
    }
    else{
        Close();
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


#include <fstream>
#include <thread>
#include <iostream>
#include <vector>
#include <memory.h>

ifstream *inputFile = nullptr;
std::vector<uint8_t> nalBuffer;
std::vector<uint8_t> frameBuffer;
bool getNextFrame(uint8_t*& naluData, int& naluSize){

    return true;
}

bool getNextNalu(uint8_t*& naluData, int& naluSize) {
    naluData = nullptr;
    naluSize = 0;

    if (!inputFile) {
        std::cerr << "File not opened." << std::endl;
        return false;
    }

    // 清空上一个 NALU 的数据
    nalBuffer.clear();
    // 读取文件直到找到 NALU 起始码
    while (true) {
        uint8_t byte;
        if (!inputFile->read(reinterpret_cast<char*>(&byte), 1)) {
            // 文件读取完毕
            if (nalBuffer.empty()) {
                // 已经读取完所有 NALU 数据
                return false;
            } else {
                // 最后一个 NALU 数据
                naluData = nalBuffer.data();
                naluSize = nalBuffer.size();
                return true;
            }
        }

        // 判断是否为 NALU 起始码
        nalBuffer.push_back(byte);
        if (nalBuffer.size() >= 4) {
            // 判断是否为 NALU 起始码
            if ((nalBuffer[nalBuffer.size() - 4] == 0x00 && nalBuffer[nalBuffer.size() - 3] == 0x00 &&
                 nalBuffer[nalBuffer.size() - 2] == 0x00 && nalBuffer[nalBuffer.size() - 1] == 0x01) ||
                (nalBuffer[nalBuffer.size() - 3] == 0x00 && nalBuffer[nalBuffer.size() - 2] == 0x00 &&
                 nalBuffer[nalBuffer.size() - 1] == 0x01)) {

                // 找到 NALU 起始码
                naluData = nalBuffer.data();
                if(nalBuffer[nalBuffer.size() - 4] == 0x00){
                    naluSize = nalBuffer.size() - 4;
                }else{
                    naluSize = nalBuffer.size() - 3;
                }
                
                return true;
            }
        }
    }
}

void testSendH264File(FrameCallback callback){
    if(inputFile == nullptr){
        inputFile = new ifstream("video.h264", std::ios::binary);
    }

    uint8_t* naluData = nullptr;
    int naluSize;
    if(getNextNalu(naluData, naluSize)){
        // cout<<"h264 frame size "<<naluSize<<endl;
        if(naluSize > 0){
            // printf("h264 frame %x, %x, %x \n", naluData[0],naluData[1],naluData[2]);
            uint8_t nal_unit_type = naluData[0] & 0x1F;
            // printf("!!!!!! nal_unit_type %d, size %d \n", nal_unit_type, naluSize);

            if(nal_unit_type == 7 || nal_unit_type == 1){
                if(frameBuffer.size() > 0){
                    callback((uint8_t *)frameBuffer.data(), frameBuffer.size());
                    frameBuffer.clear();
                    this_thread::sleep_for(chrono::milliseconds(66));
                }
            }

            frameBuffer.push_back(0);
            frameBuffer.push_back(0);
            frameBuffer.push_back(0);
            frameBuffer.push_back(0x01);
            for(int i = 0; i < naluSize; i++){
                frameBuffer.push_back(naluData[i]);
            }
        }
    }else{
        cout<<"read video.h264 file end"<<endl;
        inputFile->close();
        delete inputFile;
        inputFile = new ifstream("video.h264", std::ios::binary);
    }
    
}


void VideoStream::run(){
    cout<<"VideoStream::run  start"<<endl;
    while(m_opening){
        if(m_frame_callback != nullptr){
            testSendH264File(m_frame_callback);
        }
        // this_thread::sleep_for(chrono::milliseconds(40));
    }

    cout<<"VideoStream::run  end"<<endl;
    if(inputFile){
        inputFile->close();
        delete inputFile;
        inputFile = nullptr;
    }
}