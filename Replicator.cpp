#include "Replicator.h"
#include "CommandParser.h"
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using namespace std;

Replicator::Replicator(int masterPort,Store&store):masterPort(masterPort),store(store),masterFd(-1){};

void Replicator::connectToMaster(){
    masterFd = socket(AF_INET,SOCK_STREAM,0);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(masterPort);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int attempts = 0;
    while (connect(masterFd,(sockaddr*)&addr,sizeof(addr)) < 0) {
        attempts++;
        cout << "Connect to master failed, retrying... (" << attempts << ")" << endl;
        if (attempts >= 10) {
            cerr << "Could not connect to master after 10 attempts, exiting." << endl;
            exit(1);
        }
        this_thread::sleep_for(chrono::milliseconds(500));
        close(masterFd);
        masterFd = socket(AF_INET, SOCK_STREAM, 0);
    }
    cout<<"Connected to master on port "<<masterPort<<endl;
}
void Replicator::listenForCommands() {
    char buffer[1024];
    while (true) {
        int bytesRead = recv(masterFd, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) break;
        
        string raw(buffer, bytesRead); 
        vector<string> cmd = CommandParser::parse(raw);
        if (cmd.empty()) continue;
        
        transform(cmd[0].begin(), cmd[0].end(), cmd[0].begin(), ::toupper);
        
        if (cmd[0] == "SET") store.set(cmd[1], cmd[2]);
        else if (cmd[0] == "DEL") store.del(cmd[1]);
        else if (cmd[0] == "LPUSH") store.lpush(cmd[1], cmd[2]);
        else if (cmd[0] == "RPUSH") store.rpush(cmd[1], cmd[2]);
        else if (cmd[0] == "LREM") store.lrem(cmd[1], stoi(cmd[2]), cmd[3]);
    }
}

void Replicator::sendHandshake() {
    string msg = "REPLICATE\r\n";
    send(masterFd, msg.c_str(), msg.size(), 0);
}
