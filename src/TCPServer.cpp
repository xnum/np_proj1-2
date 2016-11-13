#include "TCPServer.h"

ClientBuffer::ClientBuffer()
{
    connfd = -1;
}

TCPServer::TCPServer()
{
    sockfd = -1;
}

int TCPServer::Init(int port)
{
    if (0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0))) {
        slogf(ERROR, "socket() %s\n", strerror(errno));
    }

    struct sockaddr_in sAddr;

    bzero((char*)&sAddr, sizeof(sAddr));
    sAddr.sin_family = AF_INET;
    sAddr.sin_addr.s_addr = INADDR_ANY;
    sAddr.sin_port = htons(port);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sAddr, sizeof(sAddr));

    if (0 > bind(sockfd, (struct sockaddr*)&sAddr, sizeof(sAddr))) {
        slogf(ERROR, "bind() %s\n", strerror(errno));
    }

    if (0 > listen(sockfd, 5)) {
        slogf(ERROR, "listen() %s\n", strerror(errno));
    }

    make_socket_non_blocking(sockfd);

    slogf(DEBUG, "success: return sockfd %d\n", sockfd);

    epoll_fd = epoll_create(50);
    if (epoll_fd < 0)
        slogf(ERROR, "epoll_create %s\n", strerror(errno));

    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLET;
    if (0 > epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &event))
        slogf(ERROR, "epoll_ctl %s\n", strerror(errno));
    events = (epoll_event*)calloc(50, sizeof(event));

    return T_Success;
}

int TCPServer::GetRequest(string& line, int& connfd)
{
    while (1) {
        recv_data_from_socket();
        for (auto& it : client_buffers) {
            auto& buffer = it.second.buffer;
            auto pos = buffer.find_first_of('\n');
            if (string::npos != pos) {
                connfd = it.first;
                line = buffer.substr(0, pos + 1);
                if (pos + 2 >= buffer.size())
                    buffer = "";
                else
                    buffer = buffer.substr(pos + 2);
                return T_Success;
            }
        }
    }
}

int TCPServer::RemoveUser(int connfd)
{
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connfd, NULL))
        slogf(ERROR, "epoll_ctl DEL connfd %d %s\n", connfd, strerror(errno));

    for (auto it = client_info.begin(); it != client_info.end(); ++it) {
        if (it->connfd == connfd) {
            slogf(INFO, "Found %d and Remove it!\n", connfd);
            client_info.erase(it);
            break;
        }
    }

    return 0;
}

int TCPServer::make_socket_non_blocking(int sfd)
{
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        slogf(ERROR, "fcntl");
        return T_Failure;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        slogf(ERROR, "fcntl");
        return T_Failure;
    }

    return T_Success;
}

int TCPServer::recv_data_from_socket()
{
    int rc = epoll_wait(epoll_fd, events, 50, -1);
    for (int i = 0; i < rc; ++i) {
        if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || !(events[i].events & EPOLLIN)) {
            client_buffers.erase(events[i].data.fd);
            for (auto it = client_info.begin(); it != client_info.end(); ++it) {
                if (it->connfd == events[i].data.fd) {
                    client_info.erase(it);
                    break;
                }
            }
            close(events[i].data.fd);
            continue;
        }
        if (sockfd == events[i].data.fd) {
            while (1) {
                struct sockaddr_in cAddr;
                socklen_t len = sizeof(cAddr);
                bzero((char*)&cAddr, sizeof(cAddr));

                int connfd;
                connfd = accept(sockfd, (struct sockaddr*)&cAddr, &len);
                if (connfd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    slogf(ERROR, "accept() %s\n", strerror(errno));
                }
                event.data.fd = connfd;
                event.events = EPOLLIN;
                if (0 > epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &event))
                    slogf(ERROR, "epoll_ctl");

                const char msg[] = "****************************************\n"
                                   "** Welcome to the information server. **\n"
                                   "****************************************\n"
                                   "% ";
                write(connfd, msg, sizeof(msg));

                char* ip = inet_ntoa(cAddr.sin_addr);
                client_info.emplace_back(ClientInfo());
                ClientInfo& ci = *client_info.rbegin();
                ci.connfd = connfd;
                snprintf(ci.ip, 128, "%s/%d", ip, ntohs(cAddr.sin_port));

                slogf(INFO, "New User Accepted %d\n", connfd);
            }
        } else {
            while (1) {
                int rc = 0;
                char buff[512] = {};
                rc = recv(events[i].data.fd, buff, 512, MSG_DONTWAIT);
                if (rc < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    slogf(ERROR, "read() %s\n", strerror(errno));
                }
                if (rc > 0) {
                    client_buffers[events[i].data.fd].buffer.append(buff, rc);
                } else
                    break;
            }
        }
    }
    return 0;
}
