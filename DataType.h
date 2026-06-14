#pragma once
#include<string>
#include<deque>
using namespace std;

class DataType{
public:
    virtual string type() =0;
    virtual ~DataType(){}
};

class StringValue:public DataType{
public:
    string value;
    string type() override {return "string";}
};

class Listvalue:public DataType{
public:
    deque<string>values;
    string type() override {return "list";}
};

