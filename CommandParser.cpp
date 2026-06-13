#include "CommandParser.h"

using namespace std;

vector<string>CommandParser::parse(const string&raw){
    vector<string>tokens;
    vector<string>lines;

    string line;
    for(int i =0;i<raw.size();i++){
        if(raw[i] == '\r' && i+1<raw.size() && raw[i+1] == '\n'){
            if(!line.empty()) lines.push_back(line);
            line = "";
            i++;
        }else{
            line += raw[i];
        }
    }   

    for(const string&l:lines){
        if(l[0] == '*' || l[0] == '$') continue;
        tokens.push_back(l);
    }

    return tokens;

}