#include "ClientHandler.h"
#include<bits/stdc++.h>
#include<sys/socket.h>
#include<unistd.h>
#include "CommandParser.h"
using namespace std;

ClientHandler::ClientHandler(int clientFd,Store&store):clientFd(clientFd),store(store){}

void ClientHandler::handle(){
    char buffer[1024];
    while(true){
        int bytesRead =recv(clientFd,buffer,sizeof(buffer),0);
        if(bytesRead<=0){
            close(clientFd);
            break;
        }
        string raw(buffer,bytesRead);
        vector<string>cmd = CommandParser::parse(raw);

        if(cmd.empty()) continue;

        string response;

        if(cmd[0] == "PING"){
            response = "+PONG\r\n";
        }
        else if(cmd[0] == "ECHO"){
            string val = cmd[1];
            response = "$" + to_string(val.size()) +"\r\n" + val + "\r\n";
        }
        else if(cmd[0] == "SET"){
            store.set(cmd[1],cmd[2]);
            if(cmd.size()>3 && (cmd[3] =="EX"||cmd[3] == "PX")){
                int ms = (cmd[3]=="EX")?stoi(cmd[4])*1000:stoi(cmd[4]);
                store.setExpiry(cmd[1],ms);
            }
            response = "+OK\r\n";
        }else if(cmd[0] == "GET"){
            string val = store.get(cmd[1]);
            if(val.empty()){
                response = "$-1\r\n";
            }else{
                response = "$" + to_string(val.size()) + "\r\n" + val +"\r\n";
            } 
        }else if(cmd[0] == "DEL"){
            store.del(cmd[1]);
            response = ":1\r\n";

        }else if(cmd[0]=="EXISTS"){
            if(store.exists(cmd[1])){
                response = ":1\r\n";
            }else{
                response = ":0\r\n";
            }
        }
        else if (cmd[0] == "LPUSH") {
            store.lpush(cmd[1], cmd[2]);
            response = ":" + to_string(store.llen(cmd[1])) + "\r\n";
        }
        else if (cmd[0] == "RPUSH") {
            store.rpush(cmd[1], cmd[2]);
            response = ":" + to_string(store.llen(cmd[1])) + "\r\n";
        }
        else if (cmd[0] == "LLEN") {
            response = ":" + to_string(store.llen(cmd[1])) + "\r\n";
        }
        else if (cmd[0] == "LRANGE") {
            vector<string> vals = store.lrange(cmd[1], stoi(cmd[2]), stoi(cmd[3]));
            response = "*" + to_string(vals.size()) + "\r\n";
            for (const string& v : vals) {
                response += "$" + to_string(v.size()) + "\r\n" + v + "\r\n";
            }
        }
        else if (cmd[0] == "LREM") {
            store.lrem(cmd[1], stoi(cmd[2]), cmd[3]);
            response = "+OK\r\n";
        }
        else{
            response = "-ERR unknown command\r\n";
        }

        send(clientFd,response.c_str(),response.size(),0);
    }
     
}