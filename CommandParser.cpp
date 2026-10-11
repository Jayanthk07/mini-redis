#include "CommandParser.h"
using namespace std;

pair<vector<string>, size_t> CommandParser::parse(const string &raw) {
    vector<string> tokens;
    if (raw.empty()) return {tokens, 0};

    if (raw[0] == '*') {
        size_t pos = 0, len = raw.size();
        size_t crlf = raw.find("\r\n", pos);
        if (crlf == string::npos) return {tokens, 0};
        int count;
        try { count = stoi(raw.substr(pos + 1, crlf - pos - 1)); }
        catch (...) { return {tokens, 0}; }
        pos = crlf + 2;

        for (int i = 0; i < count; i++) {
            if (pos >= len || raw[pos] != '$') return {tokens, 0};
            crlf = raw.find("\r\n", pos);
            if (crlf == string::npos) return {tokens, 0};
            int bulkLen;
            try { bulkLen = stoi(raw.substr(pos + 1, crlf - pos - 1)); }
            catch (...) { return {tokens, 0}; }
            pos = crlf + 2;
            if (bulkLen < 0 || pos + (size_t)bulkLen > len) return {tokens, 0};
            tokens.push_back(raw.substr(pos, bulkLen));
            pos += bulkLen;
            if (pos + 2 <= len && raw[pos] == '\r' && raw[pos + 1] == '\n') pos += 2;
            else return {tokens, 0};
        }
        return {tokens, pos};
    }

    size_t crlf = raw.find("\r\n");
    if (crlf == string::npos) return {tokens, 0};
    
    string line = raw.substr(0, crlf);
    stringstream ss(line);
    string word;
    while (ss >> word) tokens.push_back(word);
    return {tokens, crlf + 2};
}