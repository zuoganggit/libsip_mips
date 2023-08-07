#ifndef __T21_DEF__
#define __T21_DEF__
#include <iostream>

#define REMOTE_ADDR "127.0.0.1"

#define VIDEO_LOCAL_PORT  15612
#define VIDEO_REMOTE_PORT 15602

#define AUDIO_LOCAL_PORT  15613
#define AUDIO_REMOTE_PORT 15603

#pragma pack(1)
typedef struct{
   uint8_t GroupCode:8;
   uint16_t CommandID:16;
   uint8_t Version:8;
   uint32_t CommandFlag:32;
   uint16_t TotalSegment:16;
   uint16_t SubSegment:16;
   uint16_t SegmentFlag:16;
   uint16_t Reserved1:16;
   uint32_t Reserved2:32;
   uint8_t Payload[0];
}T21_Data;

typedef enum{
    DB_CMD_Play_Request = 0x01,
    DB_CMD_Play_Result = 0x02,
    DB_CMD_Stop_Request = 0x03,
    DB_CMD_Stop_Result = 0x04,
    DB_CMD_Send_Media_Request = 0x05,
    DB_CMD_Send_Media_Result = 0x06,
    DB_CMD_SetVolume_Request = 0x07,
    DB_CMD_SetVideoMode_Request = 0x09,
    DB_CMD_SetVideoAttr_Request = 0x0B,
    DB_CMD_DOControl_Request = 0x0D,
    DB_CMD_DOControl_Result = 0x0E,
    DB_CMD_HangUp_Request = 0x14,
    DB_CMD_Send_Media_Request_EX=0x1D,
    DB_CMD_Send_Media_Request_EX2 = 0x20,
    DB_CMD_Query_RegStatus_Request = 0x25,
    DB_CMD_Query_RegStatus_Result = 0x26,
    DB_CMD_Call_Request = 0xF1,
    DB_CMD_Call_Result = 0xF2,
    DB_CMD_Tunnel_Request = 0x1E,
    DB_CMD_Tunnel_Result = 0x1F,
    DB_CMD_Direct_Call_Request = 0x40,
    DB_CMD_Direct_Call_Result = 0x41,
    DB_CMD_Answer_Request = 0x42,
    DB_CMD_Answer_Result = 0x43
}T21_Cmd_Type;


typedef enum
{
    DB_MediaMode_VIDEO		    = 0x00000001,	//视频数据
    DB_MediaMode_AUDIO_Capture	= 0x00000002,	//音频输入
    DB_MediaMode_AUDIO_Play	= 0x00000004	//音频播放
} MediaMode_e;


typedef enum
{
	ET_PCM   = 3,
    ET_G711U = 5,
    ET_G711A = 6,
    ET_G722 = 8,
    ET_OPUS = 9,
} AudioEncodeType_e;


typedef enum
{
    DB_Result_Success		    = 0x0000,	//成功
    DB_Result_Failed		    = 0x0001,	//失败
    DB_Result_Calling			= 0x0002,//正在呼叫
    DB_Result_Talking			= 0x0003,//正在通话
    DB_Result_HangUp			= 0x0005,//挂断
    DB_Result_Busy			= 0x0006,//设备占线
    DB_Result_TimeOut		= 0x0007,//呼叫超时
} Result_e;


typedef enum
{
	REGST_SUCCESS		= 0x01,	// 注册服务器成功
    REGST_TRYING		= 0x02, 	// 正在连接
    REGST_AUTH_FAILED	= 0x03,  // 认证失败
    REGST_WRONG_CFG 	= 0x04,   // 配置错误,未配置参数或者参数不合法
 } RegStatus_e;


//打开/关闭音频视频 通道
//CommandID: DB_CMD_Play_Request
typedef struct{
    uint32_t	m_channelid;	// 设备通道ID
    uint8_t	    m_audioencodetype;	// 音频编码类型,见AudioEncodeType_e
	uint32_t	m_mediamode;	// 播放类型, 见MediaMode_e
}T21_Ctrl_Open_Media_Payload;

//CommandID: DB_CMD_Stop_Request
typedef struct{
    uint32_t	m_channelid;	// 设备通道ID
	uint32_t	m_mediamode;	// 播放类型, 见MediaMode_e
}T21_Ctrl_Close_Media_Payload;


//CommandID: DB_CMD_Play_Result DB_CMD_Stop_Result
typedef struct{
    uint32_t	m_result;	    // 请求回应的状态码, 见Result_e
                            // 可能的值有，1，成功，2，占线，3设备挂断。
    uint32_t	m_channelid;	// 设备通道ID
    uint32_t	m_mediamode;	// 播放类型, 见MediaMode_e
}T21_Ctrl_Media_Res_Payload;



//T21的呼叫通知
//CommandID: DB_CMD_Call_Request = 0xF1
typedef struct{
    uint8_t m_DIType;		//输入类型，取值[1-6]
                            //每个输入对应一个账号
                            //也可能多个输入对应同一个账号
}T21_Call_Req_Payload;

//CommandID: DB_CMD_Call_Result = 0xF2
typedef struct{
    uint32_t m_result;	    // 请求回应的状态码, 见Result_e
}T21_Call_Res_Payload;

typedef struct{
    uint32_t m_result;	    // 请求回应的状态码, 见Result_e
}T21_Answer_Call_Res_Payload;

//CommandID:  DB_CMD_Query_RegStatus_Result
typedef struct{
    uint8_t m_regStatus; //RegStatus_e
}T21_Query_RegStatus_Res_Payload;

//发送音频视频数据paylaod
//CommandID:DB_CMD_Send_Media_Request
typedef struct{
    uint32_t	m_channelid:32;	// 设备通道ID
    uint32_t	m_sequence:32;	// 音视频数据总分片索引,第一个分片开始从1依次递增
    uint8_t	m_iskeyframe:8;	// 是否为关键帧
    uint32_t	m_medialength:32;	// 音视频数据长度
    uint8_t	    m_mediadata[0];	// 音视频数据
}T21_Send_Media_Req_Payload;

//发送音频视频数据paylaod
//CommandID:DB_CMD_Send_Media_Request_EX
typedef struct{
    uint32_t	m_channelid:32;	// 设备通道ID
    uint32_t	m_sequence:32;	// 音视频数据总分片索引,第一个分片开始从1依次递增
    uint8_t	    m_iskeyframe:8;	// 是否为关键帧
    uint8_t	    m_reseverd0:8;	// 保留字段，必须置0
    uint32_t	m_medialength:32;	// 音视频数据长度
    uint8_t	    m_mediadata[0];	// 音视频数据
}T21_Send_Media_ReqEx_Payload;


//发送音频视频数据paylaod
//CommandID: DB_CMD_Send_Media_Request_EX2 = 0x20
typedef struct{
    uint32_t	m_channelid:32;	// 设备通道ID
    uint32_t	m_sequence:32;	// 音视频数据总分片索引,第一个分片开始从1依次递增
    uint8_t	    m_iskeyframe:8;	// 是否为关键帧
    uint8_t	    m_reseverd0:8;	// 保留字段，必须置0
    uint16_t	m_reseverd1:16;	// 保留字段，必须置0
    uint32_t	m_medialength:32;	// 音视频数据长度
    uint8_t	    m_mediadata[0];	// 音视频数据
}T21_Send_Media_ReqEx2_Payload;
//CommandID: DB_CMD_Send_Media_Result = 0x06
typedef struct{
   uint32_t	m_sequence;	// 上面请求包中的sequence
}T21_Send_Media_Res_Payload;

//开锁请求
//CommandID:DB_CMD_DOControl_Request = 0x0D
typedef struct{
    uint32_t  m_DOType; //取值[1, 2]	
}T21_OpenMutex_Req_Payload;
//CommandID:DB_CMD_ DOControl_Result = 0x0E
typedef struct{
    uint32_t  m_result;  // 请求回应的状态码, 见Result_e
}T21_OpenMutex_Res_Payload;


//DB_CMD_HangUp_Request
typedef struct{
    uint32_t  m_reson; // 挂断的原因：1=用户强制挂断；
}T21_HangUp_Req_Payload;


typedef struct{
    uint32_t	m_tunnelmode;	    //透传模式(类似于DTMF具体的传递方式)
    uint32_t	m_datalen;	    //透传数据的长度
    uint8_t	    m_data[0];	// 透传数据
}T21_DB_CMD_Tunnel_Req_Payload;

typedef struct{
    uint32_t	m_tunnelmode;	    //透传模式(类似于DTMF具体的传递方式)
    uint32_t	m_datalen;	    //透传数据的长度
    uint8_t	    m_data[0];	// 透传数据
}T21_DB_CMD_Tunnel_Res_Payload;


// DB_CMD_Direct_Call_Request = 0x40
typedef struct{
    uint8_t m_numtype; //0:号码;   1:ip地址
    uint8_t	m_callnum[16];
    uint32_t m_port;
}T21_DB_CMD_Direct_Call_Req_Payload;

//DB_CMD_Direct_Call_Result
typedef struct{
    uint32_t m_result;
}T21_DB_CMD_Direct_Call_Res_Payload;

#pragma pack()
#endif