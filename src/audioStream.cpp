#include <iostream>
#include "audioStream.h"
#include "T21_Def.h"


AudioStream::AudioStream(int t21port){
    m_opening = false;
    m_t21_port = t21port;
    m_frame_callback = nullptr;
}
    
AudioStream::~AudioStream(){
    Close();
}

shared_ptr<AudioStream> AudioStream::GetInstance(int t21port){
    static auto stream_ptr = make_shared<AudioStream>(t21port);
    return stream_ptr;
}


bool AudioStream::Init(){
    //start udp t21port service
    //recv audio pcm stream
    return true;
}
void AudioStream::Open(FrameCallback callback){
    if(!m_opening){
        m_opening = true;
        //send open Audio Stream to T21
    }
    else{
        Close();
    }

    m_frame_callback = callback;
    m_stream_run_future = std::async(std::launch::async, [this](){
        this->run();
    });
}
bool AudioStream::IsOpened(){
    return m_opening;
}
void AudioStream::Close(){
    if(!m_opening) return;

    //send close Audio Stream to T21
    m_opening = false;
    m_stream_run_future.wait();
    m_frame_callback = nullptr;
}
void AudioStream::WriteAudioFrame(uint8_t data, int dataSize){

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

void AudioStream::run(){
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


void AudioStream::run_test(){

}