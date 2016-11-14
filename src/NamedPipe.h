#pragma once

/* POSIX FIFO
 * state == EMPTY
 *
 * @BuildPipe()
 *      1. mkfifo(PATH, 0644)    [ANY SIDE]
 *      state == WAIT_RD_OPEN
 *      2. open(PATH, O_WRONLY)  [Write End]
 *
 *      ## blocks until [Read End] open (Notify through Signal)
 *
 *      3. open(PATH, O_RDONLY)  [Read End]
 *      state == ESTABLISHED (Exists)
 *
 * @GetWriteFD()
 *      ## Task done with [Write End]
 *
 * 4. close() [Write End]
 *
 * @GetReadFD()
 *      state == READ
 *      ## Task done with [Read End]
 *
 * 5. close() [Read End]
 *
 * @Free()
 *      ## cleanup which state == READ ,set to EMPTY
 *
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <atomic>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Logger.h"

#define USER_LIM 30

#define NP_EMPTY 0
#define NP_WAIT_RD_OPEN 1
#define NP_ESTABLISHED 2
#define NP_READ 3

using namespace std;

class NamedPipe {
public:
    int status;
    int fd[2];
    char path[256];
};

class NamedPipePack {
public:
    NamedPipe np[USER_LIM][USER_LIM];
};

class NamedPipeManager {
public:
    NamedPipeManager();
    ~NamedPipeManager();
    int BuildPipe(int from, int to);
    int GetWriteFD(int from, int to);
    int GetReadFD(int from, int to);
    int Free(int self_index);

    int GetIndexNeedNotify(int arr[USER_LIM]);
    int OpenReadFD(int self_index);

private:
    NamedPipePack *data;
};
