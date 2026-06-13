#include "Store.h"
#include <string>
using namespace std;

void Store::set(const string &key, const string &value)
{
    lock_guard<mutex> lock(mtx);
    data[key] = value;
}

string Store::get(const string &key)
{
    lock_guard<mutex> lock(mtx);

    if(expiry.find(key) != expiry.end() && chrono::steady_clock::now()>expiry[key]){
        data.erase(key);
        expiry.erase(key);
        return "";
    }

    if (data.find(key) != data.end()) return data[key];
    return "";
}

void Store::del(const string&key){
    lock_guard<mutex>lock(mtx);
    data.erase(key);
    expiry.erase(key);
}

void Store::setExpiry(const string&key,int milliseconds){
    lock_guard<mutex>lock(mtx);
    expiry[key] = chrono::steady_clock::now()+chrono::milliseconds(milliseconds);
}

bool Store::exists(const string&key){
    lock_guard<mutex>lock(mtx);
    if(expiry.find(key) != expiry.end() && chrono::steady_clock::now()>expiry[key]){
        data.erase(key);
        expiry.erase(key);
        return false;
    }
    return data.count(key);
}

bool Store::isExpired(const string&key){
    if(expiry.find(key)==expiry.end()) return false;
    return chrono::steady_clock::now() >expiry[key];
}