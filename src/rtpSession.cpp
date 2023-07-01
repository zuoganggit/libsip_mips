#include <cstring>
#include <memory>
#include <vector>

#include "rtpSession.h"
#include "audioStream.h"
extern "C"{
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
}

class Buffer{
public:
    Buffer(int size){
        s_buffer = new uint8_t[size];
        s_size = size;
    }
    ~Buffer(){
        delete[] s_buffer;
    }
    int s_size;
    uint8_t *s_buffer;
};

shared_ptr<Buffer> PackRTPPacket(uint16_t sequenceNumber, uint32_t timestamp, bool mark, uint8_t payloadType, const uint8_t* payload, size_t payloadSize) {
    auto rtp_data = make_shared<Buffer>(12+payloadSize);
    uint8_t *buffer = rtp_data->s_buffer;
    // RTP 头部长度为 12 字节
    uint8_t version = 2;
    uint8_t padding = 0;
    uint8_t extension = 0;
    uint8_t csrcCount = 0;
    uint8_t marker = mark ? 1 : 0;

    // 将字段按位组合成头部字节
    uint8_t byte1 = (version << 6) | (padding << 5) | (extension << 4) | csrcCount;
    uint8_t byte2 = (marker << 7) | payloadType;

    // 将头部字节写入缓冲区
    buffer[0] = byte1;
    buffer[1] = byte2;

    // 序列号和时间戳以网络字节序存储
    buffer[2] = sequenceNumber >> 8;
    buffer[3] = sequenceNumber & 0xFF;
    buffer[4] = timestamp >> 24;
    buffer[5] = (timestamp >> 16) & 0xFF;
    buffer[6] = (timestamp >> 8) & 0xFF;
    buffer[7] = timestamp & 0xFF;

    // SSRC (同步源标识符) 字段为 0
    buffer[8] = 1;
    buffer[9] = 2;
    buffer[10] = 3;
    buffer[11] = 4;

    // 将负载数据复制到缓冲区
    memcpy(buffer + 12, payload, payloadSize);
    return rtp_data;
}



RtpSession::RtpSession(int localPort, RtpSessionType type, uint8_t payloadType, FrameCallback readAudioData){
    m_remote_port = 0;
    m_local_port= localPort;
    m_audio_data_callback = readAudioData;
    m_session_type = type;
    m_socket = -1;
    m_sequenceNumber = 0;
    m_timestamp = 0;
    m_mark = true;
    if(type == PCM){
        m_payloadType = payloadType;
    }else{
        m_payloadType = payloadType;
        m_timestamp = 6000;
    }
    
    Start();
}
RtpSession::~RtpSession(){
    Stop();
}
bool RtpSession::Start(){
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置本地地址
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(m_local_port);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // 绑定本地地址
    if (bind(sock, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        std::cerr << "RtpSession Failed to bind socket to local address port " << m_local_port<<endl;
        return false;
    }

    m_socket = sock;

    m_run_future = std::async(std::launch::async, [this](){
        this->run();
    });
    return true;
}
bool RtpSession::Stop(){
    if(m_socket > 0){
        close(m_socket);
        m_socket = -1;
    }

    m_run_future.wait();
    return true;
}
void RtpSession::SetRemoteAddr(const string& dstAddr, int dstPort){
    sockaddr_in destinationAddr{};
    destinationAddr.sin_family = AF_INET;
    destinationAddr.sin_port = htons(dstPort);
    if (inet_pton(AF_INET, dstAddr.c_str(), &(destinationAddr.sin_addr)) <= 0) {
        std::cerr << "Invalid destination IP address" << std::endl;
    }else{
        m_destinationAddr = destinationAddr;
    }
    m_mark = true;
}


#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define DEFAULT_MTU 1440

void RtpSession::h264_Frag_fu_a(uint8_t *nal, int fragsize, bool mark)
{
    int start = 1, fraglen;
    uint8_t fu_header, buf[DEFAULT_MTU];

    buf[0] = (nal[0] & 0xe0) | 28; // fu_indicator
    fu_header = nal[0] & 0x1f;
    nal++;
    fragsize--;
    while(fragsize>0) {
        buf[1] = fu_header;
        if (start) {
            start = 0;
            buf[1] = fu_header | (1<<7);
        }
        fraglen = MIN(DEFAULT_MTU-2, fragsize);
        if (fraglen == fragsize) {
            buf[1] = fu_header | (1<<6);
        }

        
        memcpy(buf + 2, nal, fraglen);
        
        bool isMark = false;
        if(mark && fragsize <= fraglen){
            isMark = true;
        }
        auto buffer = PackRTPPacket(m_sequenceNumber, m_timestamp, isMark, m_payloadType, 
                        buf, fraglen + 2);
        if(m_socket > 0){
            sendto(m_socket, buffer->s_buffer, buffer->s_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
            m_sequenceNumber++;
        }

        fragsize -= fraglen;
        nal      += fraglen;
    }
}



void RtpSession::h264_Frame_RtpSend(const uint8_t* frameData, int frameSize){
    //拆分nalu 单元， rtp打包分发
    std::vector<uint8_t> nalBuffer;
    int i = 0;
    while (i < frameSize) {
        uint8_t byte = frameData[i];
        // 判断是否为 NALU 起始码
        nalBuffer.push_back(byte);
        if (nalBuffer.size() >= 4) {
            // 判断是否为 NALU 起始码
            if ((nalBuffer[nalBuffer.size() - 4] == 0x00 && nalBuffer[nalBuffer.size() - 3] == 0x00 &&
                 nalBuffer[nalBuffer.size() - 2] == 0x00 && nalBuffer[nalBuffer.size() - 1] == 0x01) ||
                (nalBuffer[nalBuffer.size() - 3] == 0x00 && nalBuffer[nalBuffer.size() - 2] == 0x00 &&
                 nalBuffer[nalBuffer.size() - 1] == 0x01)) {

                // 找到 NALU 起始码
                uint8_t* naluData = nalBuffer.data();
                int naluSize = nalBuffer.size() - 4;


                if(naluSize <= DEFAULT_MTU){
                     //直接打包发送
                     auto buffer = PackRTPPacket(m_sequenceNumber, m_timestamp, false, m_payloadType, 
                        naluData, naluSize);
                     if(m_socket > 0){
                        sendto(m_socket, buffer->s_buffer, buffer->s_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
                        m_sequenceNumber++;
                     }
                }else{ //分片发送
                    h264_Frag_fu_a(naluData, naluSize, false);
                }
                nalBuffer.clear();
            }
        }
        i++;
    }

    //最后一个nalu单元
    uint8_t* naluData = nalBuffer.data();
    int naluSize = nalBuffer.size() - 4;
    if(naluSize <= DEFAULT_MTU){
        //直接打包发送
        auto buffer = PackRTPPacket(m_sequenceNumber, m_timestamp, true, m_payloadType, 
                        naluData, naluSize);
        if(m_socket > 0){
            sendto(m_socket, buffer->s_buffer, buffer->s_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
            m_sequenceNumber++;
        }
    }else{ //分片发送
        h264_Frag_fu_a(naluData, naluSize, true);
    }
    nalBuffer.clear();
}

void RtpSession::BuildRtpAndSend(
    const uint8_t* frame, size_t size){
    if(m_session_type == PCM){
        // printf("send pcm data size %d\n", size);
        auto buffer = PackRTPPacket(m_sequenceNumber, m_timestamp, m_mark, m_payloadType, 
            frame, size);
        if(m_socket > 0){
            sendto(m_socket, buffer->s_buffer, buffer->s_size, 0, (struct sockaddr*)&m_destinationAddr, sizeof(m_destinationAddr));
            m_sequenceNumber++;
            if(size == 320){
                m_timestamp += 320;
            }
            else if(size == 160){
                m_timestamp += 160;
            }
            if(m_mark){
                m_mark = false;
            }
        }
    }else if(m_session_type == H264){
        h264_Frame_RtpSend(frame, size);
        m_timestamp += 6000;
    }
}




struct RTPHeader {
    uint16_t version : 2;
    uint16_t padding : 1;
    uint16_t extension : 1;
    uint16_t csrcCount : 4;
    uint16_t marker : 1;
    uint16_t payloadType : 7;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
};

void RtpSession::run(){
    //recv audio rtp data
    if(m_session_type == PCM){
        int buffer_size = 1550;
        uint8_t buffer[buffer_size] = {0};
        struct sockaddr_in remoteAddr{};
        socklen_t addrLen = sizeof(remoteAddr);

        // 设置阻塞超时时间
        timeval timeout{};
        timeout.tv_sec = 1;  // 设置超时时间为1秒
        timeout.tv_usec = 0;
        
        if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout)) < 0) {
            std::cerr << "Failed to set receive timeout" << std::endl;
        }
        while(m_socket > 0){
            ssize_t bytesRead = recvfrom(m_socket, buffer, buffer_size, 0, (struct sockaddr*)&remoteAddr, &addrLen);
            // if (bytesRead < 0) {
            //     std::cerr << "Failed to receive data" << std::endl;
            //     // break;
            // }

            // 解析RTP头部
            if (bytesRead > 0 && bytesRead >= sizeof(RTPHeader)) {
                // cout<<"recv rtp size "<<bytesRead<<endl;
                RTPHeader* rtpHeader = reinterpret_cast<RTPHeader*>(buffer);

                // 根据需要访问RTP头部字段
                uint16_t payloadType = ntohs(rtpHeader->payloadType);

                // 计算负载内容的起始位置
                uint8_t* payload = buffer + sizeof(RTPHeader);
                if(rtpHeader->payloadType != m_payloadType){
                    cout<<"payloadType "<<rtpHeader->payloadType<<endl;
                }
                AudioStream::GetInstance()->WriteAudioFrame(payload, bytesRead-sizeof(RTPHeader));
            }
        }
    }
}