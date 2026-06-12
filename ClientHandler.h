#pragma once
#include"Store.h"


class ClientHandler{
private:
    int clientFd;
    Store& store;
public:
    ClientHandler(int clientFd,Store& store);
    void Handle();

};