#pragma once
#include<bits/stdc++.h>
#include<unordered_map>
#include<mutex>
#include<shared_mutex>
#include<chrono>
#include<thread>
#include<atomic>
#include<condition_variable>
#include "DataType.h"
using namespace std;

class Store{
private:
    unordered_map<string,unique_ptr<DataType>>data;
    unordered_map<string,chrono::steady_clock::time_point>expiry;
    shared_mutex mtx;
    bool isExpired(const string&key);

    // Active eviction properties
    thread evictionThread;
    atomic<bool> stopEviction;
    condition_variable cv;
    mutex cv_mtx;
    void activeEvictionLoop();

public:
    Store();
    ~Store();
    void set(const string &key,const string &value);
    string get(const string &key);
    bool del(const string&key);
    bool exists(const string&key);
    void setExpiry(const string&key,int milliseconds);
    void lpush(const string&key,const string&val);
    void rpush(const string&key,const string&val);
    vector<string>lrange(const string&key,int start,int end);
    int llen(const string&key);
    void lrem(const string&key,int count,const string&val);
};