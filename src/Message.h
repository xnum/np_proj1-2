#pragma once
#include <execinfo.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <atomic>
#include <pthread.h>
#include "Logger.h"
#include "TCPServer.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#define USER_LIM 30

#define USER_SYS 30
#define USER_ALL -1
class MessagePack {
public:
    struct ClientData {
        int connfd; /* identify user by its connfd */
        char name[128];
        char ip[128];
        atomic<bool> online;
    } clients[USER_LIM]; /* id == index */

    struct MessageBox {
        char buff[10][1025];
        int ptr;
    } msgbox[USER_LIM + 1][USER_LIM];

    struct ticket_lock {
        pthread_cond_t cond;
        pthread_mutex_t mutex;
        unsigned long queue_head, queue_tail;
    } ticket;
};

class MessageCenter {
public:
    int shm_fd;
    void* mmap_ptr;

    MessageCenter();
    ~MessageCenter();
    /* interactive with other class */
    void UpdateFromTCPServer(const vector<ClientInfo>& client_info);
    void UserComing(const char* ip);
    void UserLeft(int connfd);
    void CreatedPipe(int from_index, int to_index, const char* command);
    void ReceivePipe(int from_index, int to_index, const char* command);

    void PipeExist(int from_index, int to_index);
    void PipeNotExist(int from_index, int to_index);

    /* helper function */
    int getIndexByConnfd(int connfd);
    int getConnfdByIndex(int index);
    bool isOnline(int index)
    {
        return data->clients[index].online;
    }
    void AddMessageTo(int from_index, int to_index, const char* format, ...);
    void PrintClientDataTable();

    /* print message to clients , must only server call this */
    void DealMessage(int self_index);
    void PrintLeft(int connfd);

    /* API direct called by interface
           anybody call these function need passed its connfd
           */
    void SetName(int connfd, const char* name);
    void ShowUsers(int connfd);
    void Tell(int connfd, int to_index, const char* msg);
    void Yell(int connfd, const char* msg);

private:
    MessagePack* data;
};
