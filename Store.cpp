#include "Store.h"
#include <string>
using namespace std;

void Store::set(const string &key, const string &value)
{
    lock_guard<mutex> lock(mtx);
    StringValue*sv = new StringValue;
    sv->value = value;
    data[key] = sv;
}

string Store::get(const string &key)
{
    lock_guard<mutex> lock(mtx);

    if(expiry.find(key) != expiry.end() && chrono::steady_clock::now()>expiry[key]){
        data.erase(key);
        expiry.erase(key);
        return "";
    }
    if (data.find(key) == data.end()) return "";  // check FIRST
    StringValue*sv = dynamic_cast<StringValue*>(data[key]);
    if(!sv) return "";
    return sv->value;

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

void Store::lpush(const string&key,const string&val){
    lock_guard<mutex> lock(mtx);
    if(data.find(key)== data.end()){
        data[key] = new Listvalue();
    }
    Listvalue*lv = dynamic_cast<Listvalue*>(data[key]);
    lv->values.push_front(val);
}

void Store::rpush(const string&key,const string&val){
    lock_guard<mutex>lock(mtx);

    if(data.find(key) == data.end()){
        data[key] = new Listvalue();
    }
    Listvalue*rv = dynamic_cast<Listvalue*>(data[key]);
    rv->values.push_back(val);
}

vector<string>Store::lrange(const string&key,int start,int end){
    lock_guard<mutex>lock(mtx);
    if(data.find(key) == data.end()){
        return {};
    }
    Listvalue*lv = dynamic_cast<Listvalue*>(data[key]);
    if(!lv) return {};
    int size = lv->values.size();
    if(start<0) start = size+start;
    if(end<0) end = end + size;

    if(start<0) start =0;
    if(end>=size) end = size-1;

    vector<string>result;
    for(int i = start;i<=end;i++){
        result.push_back(lv->values[i]);
    }
    return result;
}

int Store::llen(const string&key){
    lock_guard<mutex>lock(mtx);
    if(data.find(key) == data.end()){
        return 0;
    }
    Listvalue*v = dynamic_cast<Listvalue*>(data[key]);
    if(!v) return 0;
    return v->values.size();
}

void Store::lrem(const string&key,int count,const string&val){
    lock_guard<mutex>lock(mtx);
    if(data.find(key) == data.end()){
        return;
    }
    Listvalue*lv = dynamic_cast<Listvalue*>(data[key]);
    if(!lv) return;

    int removed =0;
    auto it = lv->values.begin();
    while(it != lv->values.end()){
        if(*it == val){
            it = lv->values.erase(it);
            removed++;
            if(count !=0 && removed==count) break;
        }else{
            it++;
        }
    }
}