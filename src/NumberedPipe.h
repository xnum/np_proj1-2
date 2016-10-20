#pragma once

#include <string>
#include <sstream>

using namespace std;

class NumberedPipe {
    public:
        int redirOut;
        int redirErr;
        NumberedPipe() : redirOut(-1) ,redirErr(-1) {}
        void Print() {
            printf("redirOut = %d ,redirErr = %d\n",redirOut,redirErr);
        }
};

class NumberedPipeManager {
    public:
        NumberedPipeManager() : count(0) {} 
        int CutNumberedPipeToken(string& line);
    private:
        int count;
};
