
all:
	/root/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-g++   -Os  -g0 -s -ffunction-sections   -o mips_sipper -std=c++17 src/main.cpp src/rtpSession.cpp src/audioStream.cpp src/videoStream.cpp  src/configServer.cpp src/ctrlProtocol.cpp src/sipSession.cpp   -I./include/ -L./lib -leXosip2   -losipparser2 -losip2  -ljsoncpp -lpthread -lresolv
	/root/mips-gcc540-glibc222-64bit-r3.3.0/bin/mips-linux-gnu-g++   -Os  -g0 -s -ffunction-sections -o configWebServer -std=c++17 src/configServer.cpp  src/deviceConfigService/configWebServer.cpp -I./include/ -L./lib/ -ljsoncpp -lpthread
x86:
	g++  -o x86_sipper -std=c++17 src/main.cpp  src/rtpSession.cpp src/audioStream.cpp src/videoStream.cpp  src/configServer.cpp src/ctrlProtocol.cpp src/sipSession.cpp -I./include/   -L./lib/x86 -leXosip2   -losipparser2 -losip2  -ljsoncpp -lpthread -lresolv
	g++  -o x86_configWebServer -std=c++17 src/configServer.cpp  src/deviceConfigService/configWebServer.cpp -I./include/ -L./lib/x86 -ljsoncpp -lpthread
clean:
	rm -rf x86_sipper mips_sipper configWebServer x86_configWebServer