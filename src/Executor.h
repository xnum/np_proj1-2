#pragma once

#include <cstdio>
#include <cstdlib>
#include <set>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "Parser.h"

class Executor {
public:
    bool done;
    pid_t pid;
    Command cmdHnd;

    Executor(const Command& cmd);
};
