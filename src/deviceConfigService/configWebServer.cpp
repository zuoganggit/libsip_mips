#include "httplib.h"
#include "../configServer.h"

void httpServer(){
   httplib::Server svr;

    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "*"},
        {"Access-Control-Allow-Headers", "*"},
        {"Access-Control-Allow-Credentials", "true"},
        {"Access-Control-Allow-Credentials", "true"}
    });

   // svr.set_mount_point("/", "/root/exosip/git_libsip/libsip_mips/mips_web");
   svr.set_mount_point("/", "./mips_web");
//    svr.set_mount_point("/", "/root/exosip/git_libsip/libsip_mips/web/bootstrap-5.3.0-examples/sidebars");
   

   svr.set_file_extension_and_mimetype_mapping("css", "text/css");
   svr.set_file_extension_and_mimetype_mapping("html", "text/html");
   svr.set_file_extension_and_mimetype_mapping("mjs, js", "application/javascript");
   svr.set_file_extension_and_mimetype_mapping("json", "application/json");

    // svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
	// 	    //  res.set_content(ConfigServer::GetInstance()->GetSipConfigString(), "application/json");
    //             res.set_header("Access-Control-Allow-Origin", "*");
	// 	     });
   svr.Get("/api/sipconfig", [](const httplib::Request &, httplib::Response &res) {
		     res.set_content(ConfigServer::GetInstance()->GetSipConfigString(), "application/json");
            //  cout<<"http GET sipconfig !!!!!!! "<<endl;
		     });
   
   svr.Get("/api/audio_codec", [](const httplib::Request &, httplib::Response &res) {
		     res.set_content(ConfigServer::GetInstance()->GetAudioCodecConfigString(), "application/json");
		     });

   svr.Get("/api/netconfig", [](const httplib::Request &, httplib::Response &res) {
		     res.set_content(ConfigServer::GetInstance()->GetNetConfigString(), "application/json");
		     });

   svr.listen("0.0.0.0", 8080);
}

int main(){

   httpServer();
}