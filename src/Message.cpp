#include "Message.h"

#define MMAP_SIZE (sizeof(MessagePack))

MessageCenter::MessageCenter()
{
    slogf(INFO,"message pack size = %lu\n",sizeof(MessagePack));

    if(0 > (shm_fd = shm_open("xnum", O_RDWR | O_CREAT , S_IRWXU | S_IRWXG)))
        slogf(ERROR,"shm_open %s\n",strerror(errno));

    if(0 > ftruncate(shm_fd, MMAP_SIZE))
        slogf(ERROR,"ftruncate %s\n",strerror(errno));

    mmap_ptr = mmap(0, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(MAP_FAILED == mmap_ptr)
        slogf(ERROR,"mmap %s\n",strerror(errno));
    memset(mmap_ptr, 0, MMAP_SIZE);

    void* ptr = mmap_ptr;

    data = (MessagePack*)ptr;

    pthread_mutex_init(&data->mutex, NULL);
}

void MessageCenter::UpdateFromTCPServer(const vector<ClientInfo>& client_info)
{
    if (USER_LIM <= client_info.size())
        slogf(WARN, "clients reached user limit\n");

    vector<string> new_user_ips;
    int check_online[USER_LIM] = {};

    pthread_mutex_lock(&data->mutex);

    for (size_t i = 0; i < client_info.size(); ++i) {
        auto& curr_user = client_info[i];
        /* compare if client is in our list */
        bool find = false;
        for (size_t j = 0; j < USER_LIM && !find; ++j) {
            auto& list_user = data->clients[j];
            if (list_user.connfd == curr_user.connfd && 0 == strcmp(list_user.ip, curr_user.ip)) {
                find = true;
                check_online[j] = 1;
                break;
            }
        }

        /* new user */
        if (!find) {
            slogf(DEBUG, "New User\n");
            /* find empty slot */
            size_t i = 0;
            for (i = 0; i < USER_LIM; ++i) {
                if (data->clients[i].online == false) {
                    data->clients[i].online = true;
                    break;
                }
            }

            if (i >= USER_LIM) {
                slogf(ERROR, "Find empty slot error\n");
            }

            /* write data */
            auto& empty_slot = data->clients[i];
            empty_slot.online = true;
            empty_slot.connfd = curr_user.connfd;
            strncpy(empty_slot.ip, curr_user.ip, 127);
            strncpy(empty_slot.name, "(No Name)", 127);
            check_online[i] = 1;

            slogf(INFO, "Write To Slot #%lu(%d) and Welcome!!\n", i,
                    curr_user.connfd);

        }
    }

    pthread_mutex_unlock(&data->mutex);

    for(auto it : new_user_ips) {
        UserComing(it.c_str());
    }

    for(int i = 0; i < USER_LIM; ++i) {
        if(check_online[i] == 0 && data->clients[i].online == true)
            UserLeft(data->clients[i].connfd);
    }
}

void MessageCenter::UserComing(const char* ip)
{
    AddMessageTo(USER_SYS, USER_ALL,
            "*** User '(no name)' entered from %s. ***\n", ip);
}

void MessageCenter::UserLeft(int connfd)
{
    slogf(INFO, "User (%d) left \n", connfd);

    pthread_mutex_lock(&data->mutex);

    int index = getIndexByConnfd(connfd);

    data->clients[index].online = false;

    pthread_mutex_unlock(&data->mutex);

    AddMessageTo(USER_SYS, USER_ALL, "*** User '%s' left. ***\n",
            data->clients[index].name);

    pthread_mutex_lock(&data->mutex);

    memset(&data->clients[index], 0, sizeof(data->clients[index]));

    pthread_mutex_unlock(&data->mutex);
}

void MessageCenter::CreatedPipe(int from_index, int to_index,
        const char* command)
{
    /* from_index is self */
    AddMessageTo(from_index, USER_ALL,
            "*** %s (#%d) just piped '%s' to %s (#%d) ***\n",
            data->clients[from_index].name, from_index + 1, command,
            data->clients[to_index].name, to_index + 1);
}

void MessageCenter::ReceivePipe(int from_index, int to_index,
        const char* command)
{
    /* to_index is self */
    AddMessageTo(to_index, USER_ALL,
            "*** %s (#%d) just received from %s (#%d) by '%s' ***\n",
            data->clients[to_index].name, to_index + 1,
            data->clients[from_index].name, from_index + 1, command);
}

void MessageCenter::PipeExist(int from_index, int to_index)
{
    AddMessageTo(from_index, from_index,
            "*** Error: the pipe #%d->#%d already exists. ***\n",
            from_index + 1, to_index + 1);
}

void MessageCenter::PipeNotExist(int from_index, int to_index)
{
    AddMessageTo(to_index, to_index,
            "*** Error: the pipe #%d->#%d does not exist yet. ***\n",
            from_index + 1, to_index + 1);
}

int MessageCenter::getIndexByConnfd(int connfd)
{
    for (int i = 0; i < USER_LIM; ++i) {
        if (connfd == data->clients[i].connfd)
            return i;
    }

    slogf(ERROR, "non matched connfd %d\n", connfd);

    return -1;
}

void MessageCenter::AddMessageTo(int from_index, int to_index,
        const char* format, ...)
{
    pthread_mutex_lock(&data->mutex);

    /* append message to certain message box */
    if (to_index != USER_ALL) {
        auto& box = data->msgbox[from_index][to_index];
        if (box.ptr > 10)
            return;
        memset(box.buff[box.ptr], 0, 1025);
        va_list args;
        va_start(args, format);
        vsnprintf(box.buff[box.ptr], 1024, format, args);
        va_end(args);
        box.ptr++;
    } else {
        /* for Boardcast */
        for (size_t i = 0; i < USER_LIM; ++i) {
            if (data->clients[i].online == true) {
                auto& box = data->msgbox[from_index][i];
                if (box.ptr > 10)
                    continue;
                memset(box.buff[box.ptr], 0, 1025);
                va_list args;
                va_start(args, format);
                vsnprintf(box.buff[box.ptr], 1024, format, args);
                va_end(args);
                box.ptr++;
            }
        }
    }

    pthread_mutex_unlock(&data->mutex);
}

void MessageCenter::PrintClientDataTable()
{
    printf("|--ID--|connfd|----Name----|--------IP--------|online|\n");
    for (int i = 0; i < 4; ++i) {
        auto& cli = data->clients[i];
        printf("|%-6d|%6d|%12s|%18s|%6d|\n", i, cli.connfd, cli.name, cli.ip,
                cli.online.load());
    }
}

void MessageCenter::DealMessage()
{
    pthread_mutex_lock(&data->mutex);

    for (size_t i = 0; i < USER_LIM; ++i) {
        for (size_t j = 0; j < USER_LIM + 1; ++j) { // USER_SYS
            if (data->clients[i].online == true) {
                /* send */
                auto& box = data->msgbox[j][i];
                for (int k = 0; k < box.ptr; ++k) {
                    int rc = write(data->clients[i].connfd, box.buff[k], 1025);
                    if (rc < 0) {
                        PrintClientDataTable();
                        slogf(WARN, "write(%d) error %s\n",data->clients[i].connfd, strerror(errno));
                        // maybe disconnected
                        break;
                    }
                }
            }
            memset(&data->msgbox[j][i], 0, sizeof data->msgbox[j][i]);
        }
    }

    pthread_mutex_unlock(&data->mutex);
}

void MessageCenter::SetName(int connfd, const char* name)
{

    for (size_t i = 0; i < USER_LIM; ++i) {
        if (data->clients[i].connfd == connfd) {
            pthread_mutex_lock(&data->mutex);
            strncpy(data->clients[i].name, name, 127);
            pthread_mutex_unlock(&data->mutex);
            AddMessageTo(USER_SYS, USER_ALL, "*** User from %s is named '%s'. ***\n",
                    data->clients[i].ip, name);
        }
    }

}

void MessageCenter::ShowUsers(int connfd)
{
    pthread_mutex_lock(&data->mutex);

    dprintf(
            connfd,
            "  <sockd> <nickname>      <IP/port>                    <indicate me>\n");
    for (size_t i = 0; i < USER_LIM; ++i) {
        if (data->clients[i].online == true) {
            dprintf(connfd, "  %-8lu%-16s%-29s", i + 1, data->clients[i].name,
                    data->clients[i].ip);
            if (data->clients[i].connfd == connfd)
                dprintf(connfd, "<- me");
            dprintf(connfd, "\n");
        }
    }

    pthread_mutex_unlock(&data->mutex);
}

void MessageCenter::Tell(int connfd, int to_index, const char* msg)
{
    if (data->clients[to_index].online == false) {
        dprintf(connfd, "user #%d is not online not \n", to_index + 1);
        return;
    }

    int from_index = getIndexByConnfd(connfd);
    AddMessageTo(from_index, to_index, "*** %s told you ***:  %s\n",
            data->clients[from_index].name, msg);
}

void MessageCenter::Yell(int connfd, const char* msg)
{
    int from_index = getIndexByConnfd(connfd);
    AddMessageTo(from_index, USER_ALL, "*** %s yelled ***:  %s\n",
            data->clients[from_index].name, msg);
}
