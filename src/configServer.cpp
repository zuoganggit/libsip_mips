#include <fstream>
#include <sstream>
#include <iostream>
#include "configServer.h"
#include "json/value.h"
#include "json/reader.h"

string config_file_path = "/system/etc/config.json";

shared_ptr<ConfigServer> ConfigServer::GetInstance(){
    static auto config_ptr = make_shared<ConfigServer>(config_file_path);
    return config_ptr;
}
ConfigServer::ConfigServer(const string& file_path){
    m_config_file = file_path;
    Init();
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
    printf("config file %s\n", buffer);
    Json::Reader reader;
    Json::Value value;
    reader.parse(buffer, value);

    if(value.isMember("sip_config")){
        SipConfig sipconfig;
        if(value["sip_config"].isMember("domain_name")){
            sipconfig.m_sip_domain = value["sip_config"]["domain_name"].asString();
        }

        if(value["sip_config"].isMember("user_name")){
            sipconfig.m_sip_username = value["sip_config"]["user_name"].asString();
        }

        if(value["sip_config"].isMember("password")){
            sipconfig.m_sip_password = value["sip_config"]["password"].asString();
        }

        if(value["sip_config"].isMember("reg_expires")){
            sipconfig.m_sip_expires = value["sip_config"]["reg_expires"].asInt();
        }

        if(value["sip_config"].isMember("reg_period")){
            sipconfig.m_sip_period = value["sip_config"]["reg_period"].asInt();
        }

        if(value["sip_config"].isMember("sip_outcall_account")){
            Json::Value accounts = m_config_value["sip_config"]["sip_outcall_account"];
             if(accounts.isArray()){
                for(int i=0; i< accounts.size(); i++){
                    if(accounts[i].isMember("account")){
                        sipconfig.m_out_accounts.push_back(accounts[i]["account"].asString());
                    }
                }
             }
        }
        m_sipConfig = sipconfig;
    }

    if(value.isMember("net_config")){

    }

    if(value.isMember("codec")){
        
    }
}


bool ConfigServer::GetSipDomain(string& domain){
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("domain_name")){
        domain = m_config_value["sip_config"]["domain_name"].asString();
        return true;
    }

    return false;
}
bool ConfigServer::GetUserName(string& username){
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("user_name")){
        username = m_config_value["sip_config"]["user_name"].asString();
        return true;
    }

    return false;
}
bool ConfigServer::GetPassword(string& password){
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("password")){
        password = m_config_value["sip_config"]["password"].asString();
        return true;
    }

    return false;
}

bool ConfigServer::GetRegExpires(int& expires){
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("reg_expires")){
        expires = m_config_value["sip_config"]["reg_expires"].asInt();
        return true;
    }

    return false;
}
bool ConfigServer::GetRegPeriod(int& period){
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("reg_period")){
        period = m_config_value["sip_config"]["reg_period"].asInt();
        return true;
    }

    return false;
}

bool ConfigServer::GetOutAccount(int index, string& account){
    if(m_config_value.isMember("sip_config") && 
        m_config_value["sip_config"].isMember("sip_outcall_account")){
        Json::Value accounts = m_config_value["sip_config"]["sip_outcall_account"];
        int iIndex = index - 1;
        if(accounts.isArray() &&accounts.isValidIndex(iIndex)){
            if(accounts[iIndex].isMember("account")){
                account = accounts[iIndex]["account"].asString();
                return true;
            }
        }
    }

    return false;
}

bool ConfigServer::GetLocalAddr(string& addr){
    if(m_config_value.isMember("net_config") && 
        m_config_value["net_config"].isMember("ip")){
        addr = m_config_value["net_config"]["ip"].asString();
        std::cout<<"local addr "<< addr <<endl;
        return true;
    }

    return false;
}