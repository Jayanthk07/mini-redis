#pragma once
#include<string>
#include "Store.h"


class RedisServer{
private:
    int port;
    int serverFd;
    Store& store;

public:
    RedisServer(int port ,Store &store);

    void start();

};

