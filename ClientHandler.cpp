#include "ClientHandler.h"
#include <bits/stdc++.h>
#include <sys/socket.h>
#include <unistd.h>
#include "CommandParser.h"
using namespace std;

static bool safeStoi(const string& s, int& out) {
    try {
        out = stoi(s);
        return true;
    } catch (...) {
        return false;
    }
}

void ClientHandler::handle(int clientFd, string& buffer, Store& store, RedisServer& server) {
    while (!buffer.empty()) {
        auto [cmd, consumed] = CommandParser::parse(buffer);
        if (consumed == 0 || cmd.empty()) {
            break; 
        }
        string raw = buffer.substr(0, consumed);
        buffer.erase(0, consumed);

        transform(cmd[0].begin(), cmd[0].end(), cmd[0].begin(), ::toupper);
        string response;

        auto need = [&](size_t n){ return cmd.size() >= n; };

        if(cmd[0] == "PING"){
            response = "+PONG\r\n";
        }
        else if(cmd[0] == "ECHO"){
            if(!need(2)) response = "-ERR wrong number of arguments\r\n";
            else { string val = cmd[1]; response = "$" + to_string(val.size()) + "\r\n" + val + "\r\n"; }
        }
        else if(cmd[0] == "SET"){
            if(!need(3)) { response = "-ERR wrong number of arguments\r\n"; }
            else {
                store.set(cmd[1], cmd[2]);
                if(cmd.size() > 4 && (cmd[3] == "EX" || cmd[3] == "PX")){
                    int ms;
                    if(!safeStoi(cmd[4], ms)){
                        response = "-ERR value is not an integer or out of range\r\n";
                        send(clientFd, response.c_str(), response.size(), 0);
                        continue;
                    }
                    ms = (cmd[3] == "EX") ? ms * 1000 : ms;
                    store.setExpiry(cmd[1], ms);
                }
                response = "+OK\r\n";
                if(server.getIsMaster()) server.propagate(raw);
            }
        }
        else if(cmd[0] == "GET"){
            if(!need(2)) response = "-ERR wrong number of arguments\r\n";
            else {
                string val = store.get(cmd[1]);
                response = val.empty() ? "$-1\r\n" : "$" + to_string(val.size()) + "\r\n" + val + "\r\n";
            }
        }
        else if(cmd[0] == "DEL"){
            if(!need(2)) response = "-ERR wrong number of arguments\r\n";
            else {
                bool deleted = store.del(cmd[1]);
                response = deleted ? ":1\r\n" : ":0\r\n";
                if(deleted && server.getIsMaster()) server.propagate(raw);
            }
        }
        else if(cmd[0] == "EXISTS"){
            if(!need(2)) response = "-ERR wrong number of arguments\r\n";
            else response = store.exists(cmd[1]) ? ":1\r\n" : ":0\r\n";
        }
        else if(cmd[0] == "LPUSH"){
            if(!need(3)) response = "-ERR wrong number of arguments\r\n";
            else {
                int before = store.llen(cmd[1]);
                store.lpush(cmd[1], cmd[2]);
                int after = store.llen(cmd[1]);
                if(after == before) response = "-WRONGTYPE Operation against a key holding the wrong kind of value\r\n";
                else { response = ":" + to_string(after) + "\r\n"; if(server.getIsMaster()) server.propagate(raw); }
            }
        }
        else if(cmd[0] == "RPUSH"){
            if(!need(3)) response = "-ERR wrong number of arguments\r\n";
            else { store.rpush(cmd[1], cmd[2]); response = ":" + to_string(store.llen(cmd[1])) + "\r\n"; if(server.getIsMaster()) server.propagate(raw); }
        }
        else if(cmd[0] == "LLEN"){
            if(!need(2)) response = "-ERR wrong number of arguments\r\n";
            else response = ":" + to_string(store.llen(cmd[1])) + "\r\n";
        }
        else if(cmd[0] == "LRANGE"){
            if(!need(4)) response = "-ERR wrong number of arguments\r\n";
            else {
                int start, end;
                if(!safeStoi(cmd[2], start) || !safeStoi(cmd[3], end)){
                    response = "-ERR value is not an integer or out of range\r\n";
                } else {
                    vector<string> vals = store.lrange(cmd[1], start, end);
                    response = "*" + to_string(vals.size()) + "\r\n";
                    for(const string& v : vals) response += "$" + to_string(v.size()) + "\r\n" + v + "\r\n";
                }
            }
        }
        else if(cmd[0] == "LREM"){
            if(!need(4)) response = "-ERR wrong number of arguments\r\n";
            else {
                int count;
                if(!safeStoi(cmd[2], count)){
                    response = "-ERR value is not an integer or out of range\r\n";
                } else {
                    store.lrem(cmd[1], count, cmd[3]); response = "+OK\r\n"; if(server.getIsMaster()) server.propagate(raw);
                }
            }
        }
        else{
            response = "-ERR unknown command\r\n";
        }
        send(clientFd, response.c_str(), response.size(), 0);
    }
}