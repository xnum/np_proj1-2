#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>

#include "Logger.h"

using namespace std;

#define SERVER -1
#define CLIENT -2

enum TCPServResult {
    T_Success = 0,
    T_Failure = -1
};

class ClientBuffer {
public:
    int connfd;
    string buffer;

    ClientBuffer();
};

class ClientInfo {
public:
    int connfd;
    char ip[128];
    pid_t pid;
};

class TCPServer {
public:
    TCPServer();
    int Init(int port);
    int GetRequest(string&, int&);
    int RemoveUser(int connfd);

    int type;
    vector<ClientInfo> client_info;

private:
    int sockfd;
    int epoll_fd;
    struct epoll_event event;
    struct epoll_event* events;
    map<int, ClientBuffer> client_buffers;

    int make_socket_non_blocking(int);
    int recv_data_from_socket();
    int recycle_child();
};
