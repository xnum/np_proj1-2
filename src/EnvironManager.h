#pragma once

#include <cstring>
#include <string>
#include <map>

using namespace std;

class EnvironManager {
    public:
        EnvironManager();
        ~EnvironManager() { Free(); }
        int setenv(const string&, const string&);
        string getenv(const string&);
        char** ToEnvp();
        void Free();
    private:
        map<string,string> env_list;
        char** envp;
};
