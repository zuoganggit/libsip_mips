#ifndef __CONFIG_SERVER__
#define __CONFIG_SERVER__

#include <memory>
#include <map>
using namespace std;


class ConfigServer{
public:
    static shared_ptr<ConfigServer> GetInstance();
    ConfigServer(const string& file_path);
    ~ConfigServer();
    string GetSipDomain();
    string GetUserName();
    string GetPassword();
    int GetLocalSipPort();
    void Reset();

private:
    //key value
    map<string, string> m_config_map;
};

#endif