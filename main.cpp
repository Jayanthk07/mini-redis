#include "RedisServer.h"
#include "Store.h"
#include "Replicator.h"
#include<thread>
using namespace std;

int main(int argc,char* argv[]){
    // Store store;
    // RedisServer server(6379,store);
    // server.start();
    // return 0;
    int port = 6379;
    bool isMaster = true;
    int masterPort =-1;

    for(int i =1 ;i<argc;i++){
        if(string(argv[i]) == "--port") port = stoi(argv[++i]);
        if(string(argv[i]) == "--master") isMaster = true;
        if(string(argv[i]) == "--replica"){
            isMaster=false;
            masterPort = stoi(argv[++i]);
        }
    }
    Store store;
    RedisServer server(port,store,isMaster);


    if(!isMaster){
        Replicator*replicator = new Replicator(masterPort,store);
        replicator->connectToMaster();
        replicator->sendHandshake();
        thread t([replicator](){
            replicator->listenForCommands();
        });
        t.detach();
    }
    server.start();
    return 0;
}