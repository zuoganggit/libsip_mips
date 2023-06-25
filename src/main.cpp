
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
    cv.wait(lock, []{ return exitFlag; });
}

void SendSignal()
{
    std::unique_lock<std::mutex> lock(mtx);
    exitFlag = true;
    cv.notify_all();
}


void SignalHandler(int signal)
{
    if (signal == SIGINT)
    {
        std::cout << "Received Ctrl+C signal. Exiting..." << std::endl;
        exitFlag = true;
        SendSignal();
    }
}

int main(int argc, char ** argv){

    if (argc == 4){
        sip_server_domain = argv[1];
        user_name = argv[2];
        password = argv[3];
    }
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    std::signal(SIGHUP, SignalHandler);
    
    CtrlProtocol::GetInstance();

    auto sipSessionPtr = make_shared<SipSession>(sip_server_domain, 
        user_name, password);

    if(!sipSessionPtr->Start()){
        cout<<"SipSession start fail"<<endl;
        return -1;
    }


    cout<<"hello world"<<endl;

    // this_thread::sleep_for(chrono::seconds(5));
    // sipSessionPtr->CallOutgoing("1003");

    WaitForSignal();
    return 0;
}


