#include "CommandParser.h"
using namespace std;

vector<string> CommandParser::parse(const string &raw) {
    vector<string> tokens;
    if (raw.empty()) return tokens;

    if (raw[0] == '*') {
        // RESP multibulk array: *<count>\r\n($<len>\r\n<bytes>\r\n)*
        size_t pos = 0, len = raw.size();

        size_t crlf = raw.find("\r\n", pos);
        if (crlf == string::npos) return tokens;
        int count;
        try { count = stoi(raw.substr(pos + 1, crlf - pos - 1)); }
        catch (...) { return tokens; }
        pos = crlf + 2;

        for (int i = 0; i < count; i++) {
            if (pos >= len || raw[pos] != '$') return tokens;
            crlf = raw.find("\r\n", pos);
            if (crlf == string::npos) return tokens;
            int bulkLen;
            try { bulkLen = stoi(raw.substr(pos + 1, crlf - pos - 1)); }
            catch (...) { return tokens; }
            pos = crlf + 2;
            if (bulkLen < 0 || pos + (size_t)bulkLen > len) return tokens;
            tokens.push_back(raw.substr(pos, bulkLen)); // exact bytes, no scanning
            pos += bulkLen;
            if (pos + 2 <= len && raw[pos] == '\r' && raw[pos + 1] == '\n') pos += 2;
            else return tokens;
        }
        return tokens;
    }

    // Inline command fallback: "PING\r\n" style, single line, space-separated
    string line;
    for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') break;
        line += raw[i];
    }
    stringstream ss(line);
    string word;
    while (ss >> word) tokens.push_back(word);
    return tokens;
}