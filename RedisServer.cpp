#include "RedisServer.h"
#include<sys/socket.h>
#include<unistd.h>
#include<netinet/in.h>
#include<thread>
#include "ClientHandler.h"
#include<string>
using namespace std;

RedisServer::RedisServer(int port,Store&store,bool isMaster):port(port),store(store),isMaster(isMaster),serverFd(-1){}
void RedisServer::start(){
    serverFd = socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr= INADDR_ANY;

    if (bind(serverFd,(sockaddr*)&addr,sizeof(addr)) < 0) {
        perror("bind failed");
        exit(1);
    }
    listen(serverFd,5);
    while(true){
        int clientFd = accept(serverFd,NULL,NULL);
        if (clientFd < 0) {
            perror("accept failed");
            continue;
        }
        char buf[64];
        int n = recv(clientFd,buf,sizeof(buf),MSG_PEEK);
        if (n <= 0) { close(clientFd); continue; }
        string peek(buf,n);
        if(peek.find("REPLICATE")!=string::npos){
            recv(clientFd,buf,sizeof(buf),0);
            addReplica(clientFd);
        }else{
            thread t([clientFd,this](){
                ClientHandler handler(clientFd,store,*this);
                handler.handle();
            });
            t.detach();
        }
    }
}

void RedisServer::addReplica(int fd){
    lock_guard<mutex>lock(replicaMtx);

    replicaFds.push_back(fd);

    cout<<"Replica connected: "<<fd<<endl;
}

void RedisServer::propagate(const string &raw){
    lock_guard<mutex>lock(replicaMtx);
    for(int fd:replicaFds){
        send(fd,raw.c_str(),raw.size(),0);
    }
}
