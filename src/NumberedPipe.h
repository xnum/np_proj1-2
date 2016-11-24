#pragma once

#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>

#include "Logger.h"

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
    NumberedPipe(int a, NPType b, bool c)
        : redirCount(a)
        , type(b)
        , used(c)
    {
        fd[0] = UNINIT;
        fd[1] = UNINIT;
    }
};

class NumberedPipeConfig {
public:
    int firstStdin;
    int lastStdout;
    int lastStderr;
};

class NumberedPipeManager {
public:
    NumberedPipeManager()
    {
    }
    int CutToken(string& line, int& from_other, int& to_other);
    void Free();
    NumberedPipeConfig TakeConfig();
    void Count();
    void AddNamedPipe(int from, int to);

private:
    vector<NumberedPipe> nps;
    void add(int, NPType);
    int from_fd, to_fd;
};
