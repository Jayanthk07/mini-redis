#pragma once
#include "Store.h"
#include "RedisServer.h"
#include <string>

class ClientHandler {
public:
    static void handle(int clientFd, std::string& buffer, Store& store, RedisServer& server);
};