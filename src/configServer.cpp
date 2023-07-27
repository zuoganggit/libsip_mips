#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include "configServer.h"
#include "json/value.h"
#include "json/reader.h"
#include "json/json.h"

string config_file_path = "/system/etc/config.json";

shared_ptr<ConfigServer> ConfigServer::GetInstance(){
    static auto config_ptr = make_shared<ConfigServer>(config_file_path);
    return config_ptr;
}
ConfigServer::ConfigServer(const string& file_path){
    m_config_file = file_path;
    Init();
    loadConfig();
}
ConfigServer::~ConfigServer(){
}

void ConfigServer::Init(){
    ifstream file(m_config_file);
    char buffer[1024*2] = {0};
    file.read(buffer, sizeof(buffer));
    printf("config file %s\n", buffer);
    Json::Reader reader;
    reader.parse(buffer, m_config_value);
}

void ConfigServer::loadConfig(){
    ifstream file(m_config_file);
    char buffer[1024*2] = {0};
    file.read(buffer, sizeof(buffer));
    // printf("config file %s\n", buffer);
    Json::Reader reader;
    Json::Value value;
    reader.parse(buffer, value);

    if(value.isMember("sip_config")){
        if(value["sip_config"].isMember("domain_name")){
            m_sipConfig.m_sip_domain = value["sip_config"]["domain_name"].asString();
        }

        if(value["sip_config"].isMember("user_name")){
            m_sipConfig.m_sip_username = value["sip_config"]["user_name"].asString();
        }

        if(value["sip_config"].isMember("password")){
            m_sipConfig.m_sip_password = value["sip_config"]["password"].asString();
        }

        if(value["sip_config"].isMember("reg_expires")){
            m_sipConfig.m_sip_expires = value["sip_config"]["reg_expires"].asInt();
        }

        if(value["sip_config"].isMember("reg_period")){
            m_sipConfig.m_sip_period = value["sip_config"]["reg_period"].asInt();
        }

        if(value["sip_config"].isMember("sip_outcall_account")){
            Json::Value accounts = m_config_value["sip_config"]["sip_outcall_account"];
             if(accounts.isArray()){
                for(int i=0; i< accounts.size(); i++){
                    if(accounts[i].isMember("account")){
                        m_sipConfig.m_out_accounts.push_back(accounts[i]["account"].asString());
                    }
                }
             }
        }

        if(value["sip_config"].isMember("sip_proxy")){
            Json::Value proxy_json = value["sip_config"]["sip_proxy"];
            if(proxy_json.isMember("addr")){
                m_sipConfig.m_proxy.m_addr = proxy_json["addr"].asString();
            }
            if(proxy_json.isMember("port")){
                m_sipConfig.m_proxy.m_port = proxy_json["port"].asInt();
            }
            if(proxy_json.isMember("user_name")){
                m_sipConfig.m_proxy.m_username = proxy_json["user_name"].asString();
            }
            if(proxy_json.isMember("password")){
                m_sipConfig.m_proxy.m_password = proxy_json["password"].asString();
            }
        }
    }

    if(value.isMember("net_config")){
        if(value["net_config"].isMember("ip")){
            m_netConfig.m_ip = value["net_config"]["ip"].asString();
        }
        if(value["net_config"].isMember("gateway")){
            m_netConfig.m_gateway = value["net_config"]["gateway"].asString();
        }
        if(value["net_config"].isMember("netmask")){
            m_netConfig.m_netmask = value["net_config"]["netmask"].asString();
        }
        if(value["net_config"].isMember("DNS1")){
            m_netConfig.m_dns1 = value["net_config"]["DNS1"].asString();
        }
        if(value["net_config"].isMember("DNS2")){
            m_netConfig.m_dns2 = value["net_config"]["DNS2"].asString();
        }
    }

    if(value.isMember("audio_codec")){
        Json::Value audio_codecs = value["audio_codec"];
        vector<Codec> codecs;
        if(audio_codecs.isArray()){
            for(int i=0; i< audio_codecs.size(); i++){
                if(audio_codecs[i].isMember("codec")){
                    string codec_str = audio_codecs[i]["codec"].asString();
                    if(codec_str == "G711U"){
                        codecs.push_back(G711U);
                    }else if(codec_str == "PCM"){
                        codecs.push_back(PCM);
                    }else if(codec_str == "G711A"){
                        codecs.push_back(G711A);
                    }else if(codec_str == "G722"){
                        codecs.push_back(G722);
                    }else if(codec_str == "OPUS"){
                        codecs.push_back(OPUS);
                    }
                }
            }
            m_codecConfig.m_codec = codecs;
        }
    }
}

#if 0
void ConfigServer::syncFile(){
    Json::Value root;
    Json::Value sipconfig;
    sipconfig["domain_name"] = m_sipConfig.m_sip_domain;
    sipconfig["user_name"] = m_sipConfig.m_sip_username;
    sipconfig["password"] = m_sipConfig.m_sip_password;
    sipconfig["reg_expires"] = m_sipConfig.m_sip_expires;
    for(int i = 0; i < m_sipConfig.m_out_accounts.size(); i++){
        sipconfig["sip_outcall_account"][i]["account"] = m_sipConfig.m_out_accounts[i];
    }
    root["sip_config"] = sipconfig;

    Json::Value netconfig;
    netconfig["ip"] = m_netConfig.m_ip;
    netconfig["gateway"] = m_netConfig.m_gateway;
    netconfig["netmask"] = m_netConfig.m_netmask;
    netconfig["DNS1"] = m_netConfig.m_dns1;
    netconfig["DNS1"] = m_netConfig.m_dns2;
    root["net_config"] = netconfig;

    Json::Value codec_config;
    for(int i = 0; i < m_codecConfig.m_codec.size(); i++){
        Codec codec_enum = m_codecConfig.m_codec[i];
        if(codec_enum == G711U){
            codec_config[i]["codec"] = "G711U";
        }else if(codec_enum == PCM){
            codec_config[i]["codec"] = "PCM";
        }else if(codec_enum == G711A){
            codec_config[i]["codec"] = "G711A";
        }else if(codec_enum == G722){
            codec_config[i]["codec"] = "G722";
        }else if(codec_enum == OPUS){
            codec_config[i]["codec"] = "OPUS";
        }
    }
    root["audio_codec"] = codec_config;
    
    m_config_value = root;
    Json::StyledWriter sw;
    ofstream os;
    os.open(config_file_path);
    os << sw.write(root);
    os.close();
}

bool ConfigServer::SaveSipConfig(Json::Value& value, bool sync){
    lock_guard<mutex> guard(m_config_mutex);
    if(!value.isMember("sip_config")){
        return false;
    }
    if(value["sip_config"].isMember("domain_name")){
        m_sipConfig.m_sip_domain = value["sip_config"]["domain_name"].asString();
    }

    if(value["sip_config"].isMember("user_name")){
        m_sipConfig.m_sip_username = value["sip_config"]["user_name"].asString();
    }

    if(value["sip_config"].isMember("password")){
        m_sipConfig.m_sip_password = value["sip_config"]["password"].asString();
    }

    if(value["sip_config"].isMember("reg_expires")){
        m_sipConfig.m_sip_expires = value["sip_config"]["reg_expires"].asInt();
    }

    if(value["sip_config"].isMember("reg_period")){
        m_sipConfig.m_sip_period = value["sip_config"]["reg_period"].asInt();
    }

    if(m_config_value["sip_config"].isMember("sip_outcall_account")){
        Json::Value accounts = m_config_value["sip_config"]["sip_outcall_account"];
        if(accounts.isArray()){
            m_sipConfig.m_out_accounts.clear();
            for(int i = 0; i < accounts.size(); i++){
                if(accounts[i].isMember("account")){
                    m_sipConfig.m_out_accounts.push_back(accounts[i]["account"].asString());
                }
            }
        }
    }

    if(sync){
        syncFile();
    }
    return true;
}
bool ConfigServer::SaveNetConfig(Json::Value& value, bool sync){
    lock_guard<mutex> guard(m_config_mutex);
    if(!value.isMember("net_config")){
        return false;
    }

    if(value["net_config"].isMember("ip")){
        m_netConfig.m_ip = value["net_config"]["ip"].asString();
    }

    if(value["net_config"].isMember("gateway")){
        m_netConfig.m_gateway = value["net_config"]["gateway"].asString();
    }

    if(value["net_config"].isMember("netmask")){
        m_netConfig.m_netmask = value["net_config"]["netmask"].asString();
    }

    if(value["net_config"].isMember("DNS1")){
        m_netConfig.m_dns1 = value["net_config"]["DNS1"].asString();
    }

    if(value["net_config"].isMember("DNS2")){
        m_netConfig.m_dns2 = value["net_config"]["DNS2"].asString();
    }

    if(sync){
        syncFile();
    }
    return true;
}

bool ConfigServer::SaveCodecConfig(Json::Value& value, bool sync){
    lock_guard<mutex> guard(m_config_mutex);
    if(!value.isMember("audio_codec")){
        return false;
    }

    Json::Value audio_codecs = value["audio_codec"];
    if(!audio_codecs.isArray()){
        return false;
    }

    m_codecConfig.m_codec.clear();
    
    for(int i = 0; i < audio_codecs.size(); i++){
        if(audio_codecs[i].isMember("codec")){
            string codec_str = audio_codecs[i]["codec"].asString();
            if(codec_str == "G711U"){
                m_codecConfig.m_codec.push_back(G711U);
            }else if(codec_str == "PCM"){
                m_codecConfig.m_codec.push_back(PCM);
            }else if(codec_str == "G711A"){
                m_codecConfig.m_codec.push_back(G711A);
            }else if(codec_str == "G722"){
                m_codecConfig.m_codec.push_back(G722);
            }else if(codec_str == "OPUS"){
                m_codecConfig.m_codec.push_back(OPUS);
            }
        }
    }
    if(sync){
        syncFile();
    }
    return true;
}
#endif

bool ConfigServer::GetSipDomain(string& domain){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("domain_name")){
        domain = m_config_value["sip_config"]["domain_name"].asString();
        return true;
    }

    return false;
}
bool ConfigServer::GetUserName(string& username){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("user_name")){
        username = m_config_value["sip_config"]["user_name"].asString();
        return true;
    }

    return false;
}
bool ConfigServer::GetPassword(string& password){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("password")){
        password = m_config_value["sip_config"]["password"].asString();
        return true;
    }

    return false;
}

bool ConfigServer::GetRegExpires(int& expires){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("reg_expires")){
        expires = m_config_value["sip_config"]["reg_expires"].asInt();
        return true;
    }

    return false;
}
bool ConfigServer::GetRegPeriod(int& period){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("reg_period")){
        period = m_config_value["sip_config"]["reg_period"].asInt();
        return true;
    }

    return false;
}

bool ConfigServer::GetOutAccount(int index, string& account){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("sip_outcall_account")){
        Json::Value accounts = m_config_value["sip_config"]["sip_outcall_account"];
        int iIndex = index - 1;
        if(accounts.isArray() &&accounts.isValidIndex(iIndex)){
            if(accounts[iIndex].isMember("account")){
                account = accounts[iIndex]["account"].asString();
                if(account != ""){
                    return true;
                }
            }
        }
    }

    return false;
}

bool ConfigServer::GetLocalAddr(string& addr){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("net_config") && 
        m_config_value["net_config"].isMember("ip")){
        addr = m_config_value["net_config"]["ip"].asString();
        std::cout<<"local addr "<< addr <<endl;
        return true;
    }

    return false;
}


string ConfigServer::CodecString(Codec codec){
    if(codec == PCM){   
        return "L16";
    }else if(codec == G711U){
        return "PCMU";
    }else if(codec == G711A){
        return "PCMA";
    }

    return "PCMU";
}

vector<Codec> ConfigServer::GetAudioCodec(){
    vector<Codec> codecs;
    for(auto i :m_codecConfig.m_codec){
        if(i == PCM || i == G711A || i == G711U){
            codecs.push_back(i);
        }
    }
    
    if(codecs.empty()){
        codecs.push_back(G711U);
    }
    return codecs;
}


string ConfigServer::GetSipConfigString(){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("sip_config")){
        return m_config_value["sip_config"].toStyledString();
    }
    return "not found";
}

string ConfigServer::GetNetConfigString(){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("net_config")){
        return m_config_value["net_config"].toStyledString();
    }
    return "not found";
}

string ConfigServer::GetAudioCodecConfigString(){
    lock_guard<mutex> guard(m_config_mutex);
    if(m_config_value.isMember("audio_codec")){
        return m_config_value["audio_codec"].toStyledString();
    }
    return "not found";
}

bool isValidIPAddress(const std::string& ipAddress) {
    // 正则表达式匹配 IP 地址格式
    std::regex ipRegex(R"(^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$)");

    // 使用 std::regex_match() 函数进行匹配
    return std::regex_match(ipAddress, ipRegex);
}

bool ConfigServer::GetSipProxy(SipProxy& proxy){
    lock_guard<mutex> guard(m_config_mutex);
    if(!isValidIPAddress(m_sipConfig.m_proxy.m_addr) || m_sipConfig.m_proxy.m_port <= 0){
        return false;
    }
    proxy = m_sipConfig.m_proxy;
    return true;
}