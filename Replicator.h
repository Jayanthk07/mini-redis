#pragma once
#include<bits/stdc++.h>
#include "Store.h"
using namespace std;

class Replicator{
private:
    int masterPort;
    int masterFd;
    Store&store;

public:
    Replicator(int masterPort,Store&store);
    void connectToMaster();
    void listenForCommands();
    void sendHandshake();
};