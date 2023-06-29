#ifndef __CONFIG_SERVER__
#define __CONFIG_SERVER__

#include <memory>
#include <map>
#include "json/value.h"
using namespace std;


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
private:
    string m_config_file;
    //key value
    // map<string, string> m_config_map;
    Json::Value m_config_value;
};

#endif