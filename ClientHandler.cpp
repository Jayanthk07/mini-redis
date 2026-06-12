#include "ClientHandler.h"
#include<string>
#include<sys/socket.h>
#include<unistd.h>
using namespace std;

ClientHandler::ClientHandler(int clientFd,Store&store):clientFd(clientFd),store(store){}

void ClientHandler::Handle(){
    char buffer[1024];
    while(true){
        int bytesRead =recv(clientFd,buffer,sizeof(buffer),0);
        if(bytesRead<=0){
            close(clientFd);
            break;
        }
        string response = "+PONG\r\n";
        send(clientFd,response.c_str(),sizeof(response),0);
    }
     
}