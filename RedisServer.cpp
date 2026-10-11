#include "RedisServer.h"
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include "ClientHandler.h"
#include <string>
using namespace std;

RedisServer::RedisServer(int port,Store&store,bool isMaster):port(port),store(store),isMaster(isMaster),serverFd(-1),epollFd(-1){}

void RedisServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void RedisServer::start(){
    serverFd = socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setNonBlocking(serverFd);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr= INADDR_ANY;

    if (bind(serverFd,(sockaddr*)&addr,sizeof(addr)) < 0) {
        perror("bind failed");
        exit(1);
    }
    listen(serverFd, SOMAXCONN);

    epollFd = epoll_create1(0);
    epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = serverFd;
    epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &ev);

    epoll_event events[64];

    while(true){
        int nEvents = epoll_wait(epollFd, events, 64, -1);
        for(int i = 0; i < nEvents; i++){
            int fd = events[i].data.fd;

            if(fd == serverFd){
                // Accept new connections
                while (true) {
                    int clientFd = accept(serverFd, NULL, NULL);
                    if (clientFd < 0) break;
                    setNonBlocking(clientFd);
                    epoll_event clientEv;
                    clientEv.events = EPOLLIN | EPOLLET;
                    clientEv.data.fd = clientFd;
                    epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &clientEv);
                    clientBuffers[clientFd] = "";
                }
            } else {
                // Client data arrived
                char buf[4096];
                bool closeConn = false;
                while (true) {
                    int bytesRead = recv(fd, buf, sizeof(buf), 0);
                    if (bytesRead > 0) {
                        clientBuffers[fd].append(buf, bytesRead);
                    } else if (bytesRead == 0) {
                        closeConn = true;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        closeConn = true;
                        break;
                    }
                }

                if (closeConn) {
                    close(fd);
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
                    clientBuffers.erase(fd);
                    continue;
                }

                string& buffer = clientBuffers[fd];
                if (buffer.find("REPLICATE") == 0) {
                    addReplica(fd);
                    buffer.clear();
                    // Let it just receive propagates, stop polling from epoll main loop
                    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
                    clientBuffers.erase(fd);
                } else {
                    ClientHandler::handle(fd, buffer, store, *this);
                }
            }
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
