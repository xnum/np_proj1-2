#pragma once

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include "Logger.h"

#define USER_LIM 30

#define NP_EMPTY 0

class NamedPipe {
    public:
    int status;
    int fd[2];
};

class NamedPipeManager {
    public:
        NamedPipeManager();
        int BuildPipe(int from, int to);
        int GetWriteFD(int from, int to);
        int GetReadFD(int from, int to);
        int Free();
    private:
        NamedPipe np[USER_LIM][USER_LIM];
};
