#pragma once
#include <bits/stdc++.h>
using namespace std;
class CommandParser {
public:
    static pair<vector<string>, size_t> parse(const string &raw);
};