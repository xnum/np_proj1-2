#include "TCPServer.h"

ClientBuffer::ClientBuffer()
{
    connfd = -1;
}

TCPServer::TCPServer()
{
    sockfd = -1;
    type = SERVER;
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
#ifndef SINGLE_MODE
        recycle_child();
#endif
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
        int rc = recv_data_from_socket();
        if (rc == 0) {
            line = "";
            connfd = -1;
            return T_Success;
        }
        if (rc < 0) {
            line = "";
            connfd = -rc;
            return T_Success;
        }
    }
}

int TCPServer::RemoveUser(int connfd)
{
    for (auto it = client_info.begin(); it != client_info.end(); ++it) {
        if (it->connfd == connfd) {
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
    int rc;
    while (1) {
        rc = epoll_wait(epoll_fd, events, 50, 100);
        if (rc == -1) {
            if (errno == EINTR)
                continue;
            else {
                slogf(ERROR, "epoll_wait %s\n", strerror(errno));
            }
        } else {
            break;
        }
    }
    for (int i = 0; i < rc; ++i) {
        if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || !(events[i].events & EPOLLIN)) {
            client_buffers.erase(events[i].data.fd);
            for (auto it = client_info.begin(); it != client_info.end(); ++it) {
                if (it->connfd == events[i].data.fd) {
                    client_info.erase(it);
                    break;
                }
            }
            slogf(WARN, "close(%d) \n", events[i].data.fd);
            close(events[i].data.fd);
            continue;
        }
        if (sockfd == events[i].data.fd) {
            while (1) {
                struct sockaddr_in cAddr;
                socklen_t len = sizeof(cAddr);
                bzero((char*)&cAddr, sizeof(cAddr));

                /* accept new user */
                int connfd = accept(sockfd, (struct sockaddr*)&cAddr, &len);
                if (connfd < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    slogf(ERROR, "accept() %s\n", strerror(errno));
                }

                /* print welcome message */
                const char msg[] = "****************************************\n"
                                   "** Welcome to the information server. **\n"
                                   "****************************************\n";
                write(connfd, msg, sizeof(msg));

                /* write user data */
                char* ip = inet_ntoa(cAddr.sin_addr);
                client_info.emplace_back(ClientInfo());
                ClientInfo& ci = *client_info.rbegin();
                ci.connfd = connfd;
                ci.pid = 0;
                snprintf(ci.ip, 128, "%s/%d", ip, ntohs(cAddr.sin_port));

                slogf(INFO, "New User Accepted %d\n", connfd);

#ifdef SINGLE_MODE
                write(connfd, "% ", 2);
                /* In single_mode, just add connfd to epoll_fd */
                event.data.fd = connfd;
                event.events = EPOLLIN;
                if (0 > epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &event))
                    slogf(ERROR, "epoll_ctl");
#else
                pid_t child_pid = fork();
                if (child_pid != 0) { // parent
                    /* remember its pid and do nothing */
                    ci.pid = child_pid;
                    slogf(INFO, "Serving new client with pid %d\n", child_pid);
                } else { // child
                    type = CLIENT;
                    /* we don't need other clients connfd */
                    for (size_t i = 0; i < client_info.size(); ++i) {
                        if (connfd != client_info[i].connfd)
                            close(client_info[i].connfd);
                    }
                    close(sockfd);

                    /* cleanup epoll from parent to prevent use same structure */
                    //close(epoll_fd);
                    free(events);

                    /* create a new one, which only listen current connfd */
                    epoll_fd = epoll_create(5);
                    if (epoll_fd < 0)
                        slogf(ERROR, "epoll_create %s\n", strerror(errno));

                    event.data.fd = connfd;
                    event.events = EPOLLIN;
                    if (0 > epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &event))
                        slogf(ERROR, "epoll_ctl %s\n", strerror(errno));
                    events = (epoll_event*)calloc(5, sizeof(event));

                    return -connfd;
                }
#endif
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
    return rc;
}

int TCPServer::recycle_child()
{
    /* only SERVER contains efficient client_info list */
    if (type != SERVER)
        return 0;

    bool done = true;
    do {
        done = true;
        for (auto it = client_info.begin(); it != client_info.end(); ++it) {
            if (it->pid == 0)
                continue;
            int status = 0;
            int rc = waitpid(it->pid, &status, WNOHANG);
            if (rc == -1) {
                slogf(WARN, "waitpid %s\n", strerror(errno));
            }
            if (rc > 0) {
                int connfd = it->connfd;
                slogf(DEBUG, "child with connfd %d is done rc = %d return %d\n", connfd, rc, WEXITSTATUS(status));
                RemoveUser(connfd);
                close(connfd);
                done = false;
                break;
            }
        }
    } while (!done);

    return 0;
}
