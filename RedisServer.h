#pragma once
#include<bits/stdc++.h>
#include<netinet/in.h>
#include<unistd.h>
#include "Store.h"
using namespace std;

class RedisServer{
private:
    int port;
    int serverFd;
    Store& store;
    bool isMaster;
    vector<int>replicaFds;
    mutex replicaMtx;
public:
    RedisServer(int port ,Store &store,bool isMaster);
    void start();
    void addReplica(int fd);
    void propagate(const string&raw);
    bool getIsMaster() { return isMaster; }
};

