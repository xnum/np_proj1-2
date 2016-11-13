#include "NamedPipe.h"

static NamedPipe np_initer = { NP_EMPTY, {-1, -1}, {} };

#ifdef SINGLE_MODE

NamedPipeManager::NamedPipeManager()
{
    srand(time(NULL));
    slogf(INFO, "Set All FIFO state == EMPTY\n");
    for(size_t i = 0; i < USER_LIM; ++i) {
        for(size_t j = 0; j < USER_LIM; ++j) {
            np[i][j] = np_initer;
        }
    }
}

int NamedPipeManager::BuildPipe(int from, int to)
{
    slogf(DEBUG, "FIFO from %d to %d\n",from,to);
    auto& tunnel = np[from][to];
    if(tunnel.status != NP_EMPTY) {
        slogf(DEBUG, "This FIFO is not empty\n");
        return -1;
    }

    if(0 > pipe(tunnel.fd)) {
        slogf(WARN, "Create Pipe error %s\n",strerror(errno));
        return -1;
    }

    tunnel.status = NP_ESTABLISHED;

    return 0;
}


int NamedPipeManager::GetWriteFD(int from, int to)
{
    slogf(DEBUG, "WRITE FIFO from %d to %d\n",from,to);
    auto& tunnel = np[from][to];
    if(tunnel.status != NP_ESTABLISHED) {
        slogf(DEBUG, "This FIFO is not created\n");
        return -1;
    }

    if(0 > tunnel.fd[1]) {
        slogf(WARN, "Write FD not init\n");
        return -1;
    }

    return tunnel.fd[1];
}


int NamedPipeManager::GetReadFD(int from, int to)
{
    slogf(DEBUG, "READ FIFO from %d to %d\n",from,to);
    auto& tunnel = np[from][to];
    if(tunnel.status != NP_ESTABLISHED) {
        slogf(DEBUG, "This FIFO is not created\n");
        return -1;
    }

    if(0 > tunnel.fd[0]) {
        slogf(WARN, "Read FD not init\n");
        return -1;
    }

    tunnel.status = NP_READ;

    return tunnel.fd[0];
}

int NamedPipeManager::Free()
{
    for(size_t i = 0; i < USER_LIM; ++i) {
        for(size_t j = 0; j < USER_LIM; ++j) {
            if(np[i][j].status == NP_READ) {
                slogf(DEBUG, "Free %lu %lu\n",i,j);
                np[i][j] = np_initer;
            }
        }
    }

    return 0;
}

#else
/*
 * multi process
 * use FIFO
 */

NamedPipeManager::NamedPipeManager()
{
    srand(time(NULL));
    slogf(INFO, "Set All FIFO state == EMPTY\n");
    for(size_t i = 0; i < USER_LIM; ++i) {
        for(size_t j = 0; j < USER_LIM; ++j) {
            np[i][j] = np_initer;
        }
    }
}

int NamedPipeManager::BuildPipe(int from, int to)
{
    slogf(DEBUG, "FIFO from %d to %d\n",from,to);
    auto& tunnel = np[from][to];
    if(tunnel.status != NP_EMPTY) {
        slogf(DEBUG, "This FIFO is not empty\n");
        return -1;
    }

    char table[] = "qwertyuioplkjhgfdsazxcvbnm0987654321";
    char file[11] = {};
    for(int i = 0 ; i < 10 ; ++i) {
        file[i] = table[rand() % sizeof(table)];
    }
    sprintf(tunnel.path, "/home/num/fifo/%s",file);

    int rc = mkfifo(tunnel.path, 0644);
    if(rc < 0) {
        dprintf(ERROR, "mkfifo %s\n",strerror(errno));
    }

    tunnel.status = NP_WAIT_RD_OPEN;

    return 0;
}


int NamedPipeManager::GetWriteFD(int from, int to)
{
    /* when failed return -1, tag the pipe should be cleared */
    return 0;
}


int NamedPipeManager::GetReadFD(int from, int to)
{
    return 0;
}

int NamedPipeManager::Free()
{
    return 0;
}

#endif
