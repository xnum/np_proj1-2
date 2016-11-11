#pragma once

#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>

using namespace std;

#define UNINIT -1

enum NPType {
    OUT,
    ERR
};

class NumberedPipe {
    public:
        int redirCount;
        NPType type;
        bool used;
        int fd[2];
        NumberedPipe(int a,NPType b,bool c)
         : redirCount(a) ,type(b), used(c) {
            fd[0] = UNINIT;
            fd[1] = UNINIT;
        }
};

class NumberedPipeConfig {
    public:
        int firstStdin;
        int lastStdout;
        int lastStderr;

        int fifo_read;
        int fifo_write;
};

class NumberedPipeManager {
    public:
        NumberedPipeManager() {} 
        int CutNumberedPipeToken(string& line);
        void Free();
        NumberedPipeConfig TakeConfig();
        void Count();

        int fifo_read;
        int fifo_write;
    private:
        vector<NumberedPipe> nps;
        void add(int,NPType);

};
