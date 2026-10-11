#include "RedisServer.h"
#include "Store.h"
#include "Replicator.h"
#include<thread>
#include<memory>
using namespace std;

int main(int argc,char* argv[]){
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

    unique_ptr<Replicator> replicator;

    if(!isMaster){
        replicator = make_unique<Replicator>(masterPort,store);
        replicator->connectToMaster();
        replicator->sendHandshake();
        Replicator* repPtr = replicator.get();
        thread t([repPtr](){
            repPtr->listenForCommands();
        });
        t.detach();
    }
    server.start();
    return 0;
}