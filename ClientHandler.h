#pragma once
#include"Store.h"
#include "RedisServer.h"

class ClientHandler{
private:
    int clientFd;
    Store& store;
    RedisServer&server;
public:
    ClientHandler(int clientFd,Store& store,RedisServer&server);
    void handle();
};