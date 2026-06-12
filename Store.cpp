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
    if (data.find(key) != data.end())
    {
        return data[key];
    }
    return "";
}