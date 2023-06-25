
mips:
	/root/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-g++  -Os -s -ffunction-sections   -o mips_sipper -std=c++17 src/main.cpp src/rtpSession.cpp src/audioStream.cpp src/videoStream.cpp  src/configServer.cpp src/ctrlProtocol.cpp src/sipSession.cpp   -I./include/ -L./lib -leXosip2   -losipparser2 -losip2  -ljsoncpp -lpthread -lresolv
x86:
	g++  -o x86_sipper -std=c++17 src/main.cpp  src/rtpSession.cpp src/audioStream.cpp src/videoStream.cpp  src/configServer.cpp src/ctrlProtocol.cpp src/sipSession.cpp   -I/root/exosip/libsip_x86/include -L/root/exosip/libsip_x86/lib -leXosip2   -losipparser2 -losip2  -ljsoncpp -lpthread -lresolv -g
clean:
	rm -rf x86_sipper mips_sipper