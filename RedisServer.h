#pragma once
#include<bits/stdc++.h>
#include<netinet/in.h>
#include<unistd.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include "Store.h"
using namespace std;

class RedisServer{
private:
    int port;
    int serverFd;
    int epollFd;
    Store& store;
    bool isMaster;
    vector<int>replicaFds;
    mutex replicaMtx;
    unordered_map<int, string> clientBuffers;
    
    void setNonBlocking(int fd);
public:
    RedisServer(int port ,Store &store,bool isMaster);
    void start();
    void addReplica(int fd);
    void propagate(const string&raw);
    bool getIsMaster() { return isMaster; }
};
