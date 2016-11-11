#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <atomic>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "Logger.h"

#define CI_SIZE 30
#define MMAP_SIZE (sizeof(MessagePack)*2)
//16 * 1024 * 1024

using namespace std;

struct ClientInfo {
    int id;
    char name[128];
    char ip[128];
    int online;
};

struct MessageBuffer {
    char buff[10][1024];
    atomic<int> ptr;
};

struct Broadcast {
    char buff[1024];
    int from;
    atomic<bool> use;
};

#define NP_UNINIT 0
#define NP_OPENED 1
#define NP_WAIT 2
#define NP_OK 3
#define NP_USING 4

class NamedPipe {
    public:
    char path[128];
    atomic<int> status;
    int read_fd;
    int write_fd;

    int Init();
    int GetReadEnd();
    int GetWriteEnd();
};

struct MessagePack {
    ClientInfo client_info[30];
    MessageBuffer msg_box[30][30];
    Broadcast bc;
    int ci_ptr;
    NamedPipe pipe[30][30];
};

class MessageCenter {
    public:
        int self_index;

        MessageCenter();
        ~MessageCenter();

        int AddUser(int,char *name,char *ip);
        void ShowUsers(int id);
        void RemoveUser(int id);
        void SetName(int id,const char *name);
        
        void AddBroadCast(const char *msg,int from);
        int GetBroadCast(char *msg, int size);

        int Send(int id,const char *msg);
        int Trans(int from,int to, int to_fd);

        int OpenReadEnd();
        int OpenWriteEnd(int);
        int Notify(int,int);
        int GetReadEnd(int);
        int FreeNamedPipe(int target);

        void MentionNamedPipe(int, const char*);
        void UsingNamedPipe(int , const char* );
    private:
        MessagePack *data;

        int shm_fd;
        void *mmap_ptr;
};
