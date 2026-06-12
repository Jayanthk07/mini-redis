#pragma once
#include<string>
#include<unordered_map>
#include<mutex>
using namespace std;

class Store{
private:
    unordered_map<string,string>data;
    mutex mtx;
public:
    void set(const string &key,const string &value);
    string get(const string &key);

};