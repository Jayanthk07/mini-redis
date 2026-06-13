#include "RedisServer.h"
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<thread>
#include "ClientHandler.h"
#include<string>
using namespace std;

RedisServer::RedisServer(int port,Store&store):port(port),store(store){}

void RedisServer::start(){
    serverFd = socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr= INADDR_ANY;
    bind(serverFd,(sockaddr*)&addr,sizeof(addr));

    listen(serverFd,5);

    while(true){
        int clientFd = accept(serverFd,NULL,NULL);

        thread t([clientFd,this](){
            ClientHandler handler(clientFd,store);
            handler.handle();
        });
        t.detach();
    }

}
