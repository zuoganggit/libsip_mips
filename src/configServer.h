#ifndef __CONFIG_SERVER__
#define __CONFIG_SERVER__

#include <memory>
#include <map>
#include <vector>
#include <mutex>
#include "json/value.h"
using namespace std;

typedef struct{
    string m_addr;
    int m_port;
    string m_username;
    string m_password;
}SipProxy;

typedef struct{
    string m_sip_domain;
    string m_sip_username;
    string m_sip_password;
    int m_sip_expires;
    int m_sip_period;
    bool m_is_tcp;
    vector<string> m_out_accounts;
    SipProxy m_proxy;
    int m_anwer_sleep;
}SipConfig;


typedef struct{
    string m_ip;
    string m_gateway;
    string m_netmask;
    string m_dns1;
    string m_dns2;
}NetConfig;

typedef enum{
    PCM,
    G711U,
    G711A,
    G722,
    OPUS,
    H264
}Codec;

typedef struct{
    vector<Codec> m_codec;
}CodecConfig;

class ConfigServer{
public:
    static shared_ptr<ConfigServer> GetInstance();
    ConfigServer(const string& file_path);
    ~ConfigServer();
    void Init();
    bool GetSipDomain(string& domain);
    bool GetUserName(string& username);
    bool GetPassword(string& password);
    bool GetRegExpires(int& expires);
    bool GetRegPeriod(int& period);
    bool GetOutAccount(int index, string& account);
    bool GetLocalAddr(string& addr);
    vector<Codec> GetAudioCodec();
    string CodecString(Codec codec);
    bool GetSipProxy(SipProxy& proxy);

    // bool SaveSipConfig(Json::Value& value, bool syncFile=true);
    // bool SaveNetConfig(Json::Value& value, bool syncFile=true);
    // bool SaveAccountConfig(Json::Value& value, bool syncFile=true);
    // bool SaveCodecConfig(Json::Value& value, bool syncFile=true);
    
    bool  GetEnableSipTcp();
    string GetSipConfigString();
    string GetNetConfigString();
    string GetAudioCodecConfigString();

    int GetAnwserSleep();
private:
    void loadConfig();
    // void syncFile();
    string m_config_file;
    //key value
    // map<string, string> m_config_map;
    Json::Value m_config_value;

    // Json::Value m_sip_config;
    // Json::Value m_account_config;
    // Json::Value m_net_config;
    // Json::Value m_codec_config;

    SipConfig m_sipConfig;
    NetConfig m_netConfig;
    CodecConfig m_codecConfig;
    mutex m_config_mutex;
};

#endif