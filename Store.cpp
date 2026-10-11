#include "Store.h"
#include <string>
#include <shared_mutex>
#include <vector>
using namespace std;

Store::Store() : stopEviction(false) {
    evictionThread = thread(&Store::activeEvictionLoop, this);
}

Store::~Store() {
    stopEviction = true;
    cv.notify_all();
    if (evictionThread.joinable()) {
        evictionThread.join();
    }
}

void Store::activeEvictionLoop() {
    while (!stopEviction) {
        {
            unique_lock<mutex> lock(cv_mtx);
            // Wait for 1 second or until stopped
            cv.wait_for(lock, chrono::seconds(1), [this]{ return stopEviction.load(); });
        }
        if (stopEviction) break;
        
        vector<string> keysToDelete;
        {
            // Read-lock to quickly find expired keys without blocking writers
            shared_lock<shared_mutex> rlock(mtx);
            auto now = chrono::steady_clock::now();
            for (auto it = expiry.begin(); it != expiry.end(); ++it) {
                if (now > it->second) {
                    keysToDelete.push_back(it->first);
                }
            }
        }
        
        if (!keysToDelete.empty()) {
            // Write-lock to safely remove the expired keys
            unique_lock<shared_mutex> wlock(mtx);
            auto now = chrono::steady_clock::now();
            for (const auto& key : keysToDelete) {
                auto exp_it = expiry.find(key);
                if (exp_it != expiry.end() && now > exp_it->second) {
                    auto data_it = data.find(key);
                    if (data_it != data.end()) {
                        data.erase(data_it);
                    }
                    expiry.erase(exp_it);
                }
            }
        }
    }
}

void Store::set(const string &key, const string &value) {
    unique_lock<shared_mutex> lock(mtx);
    auto sv = make_unique<StringValue>();
    sv->value = value;
    data[key] = std::move(sv);
}

string Store::get(const string &key)
{
    shared_lock<shared_mutex> lock(mtx);
    
    if(expiry.find(key) != expiry.end() && chrono::steady_clock::now()>expiry[key]){
        lock.unlock(); // Release read lock to prevent deadlock
        unique_lock<shared_mutex> ulock(mtx); // Acquire write lock for cleanup
        if(expiry.find(key) != expiry.end() && chrono::steady_clock::now()>expiry[key]){
            data.erase(key);
            expiry.erase(key);
        }
        return "";
    }
    if (data.find(key) == data.end()) return "";
    StringValue*sv = dynamic_cast<StringValue*>(data[key].get());
    if(!sv) return "";
    return sv->value;
}

bool Store::del(const string&key){
    unique_lock<shared_mutex> lock(mtx);
    auto it = data.find(key);
    bool found = (it != data.end());
    if (found) {
        data.erase(it);
    }
    expiry.erase(key);
    return found;
}

void Store::setExpiry(const string&key,int milliseconds){
    unique_lock<shared_mutex> lock(mtx);
    expiry[key] = chrono::steady_clock::now()+chrono::milliseconds(milliseconds);
}

bool Store::exists(const string&key){
    shared_lock<shared_mutex> lock(mtx);
    if(expiry.find(key) != expiry.end() && chrono::steady_clock::now()>expiry[key]){
        lock.unlock();
        unique_lock<shared_mutex> ulock(mtx);
        if(expiry.find(key) != expiry.end() && chrono::steady_clock::now()>expiry[key]){
            data.erase(key);
            expiry.erase(key);
        }
        return false;
    }
    return data.count(key);
}

bool Store::isExpired(const string&key){
    if(expiry.find(key)==expiry.end()) return false;
    return chrono::steady_clock::now() >expiry[key];
}


void Store::lpush(const string&key,const string&val){
    unique_lock<shared_mutex> lock(mtx);
    if(data.find(key)== data.end()){
        data[key] = make_unique<Listvalue>();
    }
    Listvalue*lv = dynamic_cast<Listvalue*>(data[key].get());
    if(!lv) return; // TODO: return WRONGTYPE error to client instead of silently no-op'ing
    lv->values.push_front(val);
}

void Store::rpush(const string&key,const string&val){
    unique_lock<shared_mutex> lock(mtx);

    if(data.find(key) == data.end()){
        data[key] = make_unique<Listvalue>();
    }
    Listvalue*rv = dynamic_cast<Listvalue*>(data[key].get());
    if(!rv) return;
    rv->values.push_back(val);
}

vector<string>Store::lrange(const string&key,int start,int end){
    shared_lock<shared_mutex> lock(mtx);
    if(data.find(key) == data.end()){
        return {};
    }
    Listvalue*lv = dynamic_cast<Listvalue*>(data[key].get());
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
    shared_lock<shared_mutex> lock(mtx);
    if(data.find(key) == data.end()){
        return 0;
    }
    Listvalue*v = dynamic_cast<Listvalue*>(data[key].get());
    if(!v) return 0;
    return v->values.size();
}

void Store::lrem(const string&key,int count,const string&val){
    unique_lock<shared_mutex> lock(mtx);
    if(data.find(key) == data.end()){
        return;
    }
    Listvalue*lv = dynamic_cast<Listvalue*>(data[key].get());
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