#pragma once

#include <string>
#include <map>

using namespace std;

class EnvironManager {
    public:
        EnvironManager();
        int setenv(const string&, const string&);
        string getenv(const string&);
    private:
        map<string,string> env_list;
};
