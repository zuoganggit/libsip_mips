
#include <iostream>
#include <memory>
#include <signal.h>
#include <condition_variable>
#include <mutex>
#include <csignal>
#include <thread>
#include "sipSession.h"
#include "configServer.h"
#include "ctrlProtocol.h"

using namespace std;
string sip_server_domain = "192.168.101.6:5060";
string user_name = "1001";
string password = "1234";

std::condition_variable cv;
std::mutex mtx;

bool exitFlag = false;

void WaitForSignal()
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []{ 
        return exitFlag; 
        });
}

void SendSignal()
{
    std::unique_lock<std::mutex> lock(mtx);
    exitFlag = true;
    cv.notify_all();
}


void SignalHandler(int signal)
{
    std::cout << "Received Ctrl+C signal. Exiting..." << std::endl;
    exitFlag = true;
    SendSignal();
}

int main(int argc, char ** argv){
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGHUP, SignalHandler);
    
    if (!ConfigServer::GetInstance()->GetSipDomain(sip_server_domain)){
        cerr<<"GetSipDomain  fail"<<endl;
        return 0;
    }

    if (!ConfigServer::GetInstance()->GetUserName(user_name)){
        cerr<<"GetUserName  fail"<<endl;
        return 0;
    }

    if (!ConfigServer::GetInstance()->GetPassword(password)){
        cerr<<"GetPassword  fail"<<endl;
        return 0;
    }
    SipProxy sipproxy;
    ConfigServer::GetInstance()->GetSipProxy(sipproxy);

    cout << "sipproxy addr "<<sipproxy.m_addr<<" port "<<sipproxy.m_port
     <<" user "<<sipproxy.m_username<<" password  "<<sipproxy.m_password<<endl;
     
    CtrlProtocol::GetInstance();

    auto sipSessionPtr = SipSession::GetInstance(sip_server_domain, 
        user_name, password);

    if(!sipSessionPtr->Start()){
        cout<<"SipSession start fail"<<endl;
        return -1;
    }

    // this_thread::sleep_for(chrono::seconds(3));
    // sipSessionPtr->CallOutgoing("1003");
    // this_thread::sleep_for(chrono::seconds(3));
    // sipSessionPtr->TerminateCalling();
    WaitForSignal();
    return 0;
}


