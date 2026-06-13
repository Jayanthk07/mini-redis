#pragma once
#include<string>
#include<unordered_map>
#include<mutex>
#include<chrono>
using namespace std;

class Store{
private:
    unordered_map<string,string>data;
    unordered_map<string,chrono::steady_clock::time_point>expiry;
    mutex mtx;
    bool isExpired(const string&key);
public:
    void set(const string &key,const string &value);
    string get(const string &key);
    void del(const string&key);
    bool exists(const string&key);
    
    void setExpiry(const string&key,int milliseconds);
};