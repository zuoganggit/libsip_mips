
#include <iostream>
#include <memory>
#include <signal.h>
#include <condition_variable>
#include <mutex>
#include <csignal>
#include <thread>
// #include "httplib.h"
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


// void httpServer(){

//    httplib::Server svr;
//    svr.Get("/api/sipconfig", [](const httplib::Request &, httplib::Response &res) {
// 		     res.set_content("Hello World!", "text/plain");
// 		     });
   
//    svr.Post("/api/sipconfig", [](const httplib::Request &, httplib::Response &res) {
// 		     res.set_content("Hello World!", "text/plain");
// 		     });

//    svr.Get("/api/netconfig", [](const httplib::Request &, httplib::Response &res) {
// 		     res.set_content("Hello World!", "text/plain");
// 		     });
   
//    svr.Post("/api/netconfig", [](const httplib::Request &, httplib::Response &res) {
// 		     res.set_content("Hello World!", "text/plain");
// 		     });

//    svr.Get("/api/codecconfig", [](const httplib::Request &, httplib::Response &res) {
// 		     res.set_content("Hello World!", "text/plain");
// 		     });
   
//    svr.Post("/api/codecconfig", [](const httplib::Request &, httplib::Response &res) {
// 		     res.set_content("Hello World!", "text/plain");
// 		     });
   
//    svr.set_mount_point("/", "/root/exosip/git_libsip/libsip_mips/web/bootstrap-5.3.0-examples/");
//    svr.set_mount_point("/", "/root/exosip/git_libsip/libsip_mips/web/bootstrap-5.3.0-examples/sidebars");
   
//    svr.set_file_extension_and_mimetype_mapping("css", "text/css");
//    svr.set_file_extension_and_mimetype_mapping("html", "text/html");
//    svr.set_file_extension_and_mimetype_mapping("mjs, js", "application/javascript");
//    svr.set_file_extension_and_mimetype_mapping("json", "application/json");

//    svr.listen("0.0.0.0", 8080);
// }


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

    CtrlProtocol::GetInstance();

    auto sipSessionPtr = SipSession::GetInstance(sip_server_domain, 
        user_name, password);

    if(!sipSessionPtr->Start()){
        cout<<"SipSession start fail"<<endl;
        return -1;
    }


    cout<<"sipper start ok"<<endl;
    // this_thread::sleep_for(chrono::seconds(5));
    // sipSessionPtr->CallOutgoing("1003");
    // this_thread::sleep_for(chrono::seconds(10));
    // sipSessionPtr->TerminateCalling();
    // this_thread::sleep_for(chrono::seconds(5));
    // sipSessionPtr->CallOutgoing("1003");
    WaitForSignal();

    // httpServer();
    return 0;
}


