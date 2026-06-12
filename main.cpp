#include "RedisServer.h"
#include "Store.h"


int main(){
    Store store;
    RedisServer server(6379,store);
    server.start();
    return 0;
}