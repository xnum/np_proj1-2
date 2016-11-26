#include "NamedPipe.h"
#include "common.h"

//static NamedPipe np_initer = { NP_EMPTY, { -1, -1 }, {} };

NamedPipeManager::NamedPipeManager()
{
#ifdef SINGLE_MODE
    data = (NamedPipePack*)calloc(1, sizeof(NamedPipePack));
#else
    DIR* path = opendir(FIFO_PATH);
    if (!path) {
        slogf(ERROR, "FIFO dir %s Not Exists\n", FIFO_PATH);
    }
    closedir(path);

    data = (NamedPipePack*)mmap(NULL, sizeof(NamedPipePack), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
#endif

    srand(time(NULL));
    slogf(INFO, "Set All FIFO state == EMPTY\n");
    for (size_t i = 0; i < USER_LIM; ++i) {
        for (size_t j = 0; j < USER_LIM; ++j) {
            data->np[i][j].status = NP_EMPTY;
            data->np[i][j].fd[0] = -1;
            data->np[i][j].fd[1] = -1;
            memset(data->np[i][j].path, 0, 256);
        }
    }
}

NamedPipeManager::~NamedPipeManager()
{
#ifdef SINGLE_MODE
    free(data);
#else

#endif
}

#ifdef SINGLE_MODE
int NamedPipeManager::BuildPipe(int from, int to)
{
    slogf(DEBUG, "FIFO from %d to %d\n", from, to);
    auto& tunnel = data->np[from][to];

    if (tunnel.status == NP_EXPIRED) {
        tunnel.status = NP_EMPTY;
        tunnel.fd[0] = -1;
        tunnel.fd[1] = -1;
        memset(tunnel.path, 0, 256);
    }

    if (tunnel.status != NP_EMPTY) {
        slogf(DEBUG, "This FIFO is not empty\n");
        return -1;
    }

    if (0 > pipe(tunnel.fd)) {
        slogf(WARN, "Create Pipe error %s\n", strerror(errno));
        return -1;
    }

    tunnel.status = NP_ESTABLISHED;

    return 0;
}

int NamedPipeManager::Free(int self_index)
{
    for (size_t i = 0; i < USER_LIM; ++i) {
        for (size_t j = 0; j < USER_LIM; ++j) {
            if (data->np[i][j].status == NP_READ) {
                slogf(DEBUG, "Free %lu %lu\n", i, j);
                data->np[i][j].status = NP_EMPTY;
                data->np[i][j].fd[0] = -1;
                data->np[i][j].fd[1] = -1;
                memset(data->np[i][j].path, 0, 256);
            }
        }
    }

    return 0;
}

int NamedPipeManager::GetIndexNeedNotify(int arr[USER_LIM])
{
    return 0;
}

int NamedPipeManager::OpenReadFD(int self_index)
{
    return 0;
}

#else
/*
 * multi process
 * use FIFO
 */

int NamedPipeManager::BuildPipe(int from, int to)
{
    srand(time(NULL)%getpid());
    slogf(INFO, "Build FIFO #%d -> #%d\n", from + 1, to + 1);
    if (from == to) {
        slogf(WARN, "Don't build pipe to yourself\n");
        return -1;
    }

    auto& tunnel = data->np[from][to];

    if (tunnel.status == NP_EXPIRED) {
        tunnel.status = NP_EMPTY;
        tunnel.fd[0] = -1;
        tunnel.fd[1] = -1;
        memset(tunnel.path, 0, 256);
    }

    if (tunnel.status != NP_EMPTY) {
        slogf(DEBUG, "This FIFO is not empty (%d)\n",tunnel.status.load());
        return -1;
    }

    char table[] = "qwertyuioplkjhgfdsazxcvbnm0987654321";
    char file[11] = {};
    for (int i = 0; i < 10; ++i) {
        file[i] = table[rand() % (sizeof(table)-1)];
    }
    sprintf(tunnel.path, FIFO_PATH "/%s", file);

    int rc = mkfifo(tunnel.path, 0644);
    if (rc < 0) {
        dprintf(ERROR, "mkfifo %s\n", strerror(errno));
    }

    tunnel.status = NP_WAIT_RD_OPEN;

    slogf(INFO, "Wait open(WRONLY) %s\n",tunnel.path);
    rc = open(tunnel.path, O_WRONLY);

    if (rc < 0) {
        slogf(WARN, "open WRONLY %s\n", strerror(errno));
        tunnel.status = NP_READ;
        return -1;
    }

    slogf(INFO, "#%d -> #%d open(O_WRONLY) ok %d\n", from + 1, to + 1, rc);

    tunnel.fd[1] = rc;
    tunnel.status = NP_ESTABLISHED;

    return 0;
}

int NamedPipeManager::Free(int self_index)
{
    slogf(DEBUG, "Try Free %d\n",self_index);

    /* I'm Write end */
    for (size_t i = 0; i < USER_LIM; ++i) {
        if (data->np[self_index][i].status == NP_ESTABLISHED ||
             data->np[self_index][i].status == NP_READ ) {
            slogf(DEBUG, "Close %d %lu\n", self_index, i);
            close(data->np[self_index][i].fd[1]);
        }
    }

    /* I'm Read end */
    for (size_t i = 0; i < USER_LIM; ++i) {
        if (data->np[i][self_index].status == NP_READ) {
            slogf(DEBUG, "Free %lu %d\n", i, self_index);
            if (0 > unlink(data->np[i][self_index].path))
                slogf(WARN, "unlink %s\n", strerror(errno));
            data->np[i][self_index].status = NP_EMPTY;
            data->np[i][self_index].fd[0] = -1;
            data->np[i][self_index].fd[1] = -1;
            memset(data->np[i][self_index].path, 0, 256);
        }
    }

    return 0;
}

int NamedPipeManager::GetIndexNeedNotify(int arr[USER_LIM])
{
    int count = 0;
    for (int i = 0; i < USER_LIM; ++i) {
        /* from j to i */
        for (int j = 0; j < USER_LIM; ++j) {
            auto& tunnel = data->np[j][i];
            if (tunnel.status == NP_WAIT_RD_OPEN) {
                slogf(INFO, "#%d Need Open\n", i);
                arr[count++] = i;
                break;
            }
        }
    }

    return count;
}

int NamedPipeManager::OpenReadFD(int self_index)
{
    slogf(INFO, "Signal trigerred #%d\n", self_index);
    for (int i = 0; i < USER_LIM; ++i) {
        auto& tunnel = data->np[i][self_index];
        if (tunnel.status == NP_WAIT_RD_OPEN) {
            int rc = open(tunnel.path, O_RDONLY);
            if (rc < 0) {
                slogf(WARN, "open RDONLY %s\n", strerror(errno));
                tunnel.status = NP_READ;
                continue;
            }

            slogf(INFO, "#%d -> #%d wait open(RD) ok %d\n", i + 1, self_index + 1, rc);
            tunnel.fd[0] = rc;
            tunnel.status = NP_ESTABLISHED;
        }
    }

    return 0;
}

#endif

int NamedPipeManager::GetWriteFD(int from, int to)
{
    slogf(INFO, "#%d -> #%d\n", from + 1, to + 1);
    auto& tunnel = data->np[from][to];
    if (tunnel.status != NP_ESTABLISHED) {
        slogf(WARN, "This FIFO is not created\n");
        return -1;
    }

    if (0 > tunnel.fd[1]) {
        slogf(WARN, "Write FD not init\n");
        return -1;
    }

    return tunnel.fd[1];
}

int NamedPipeManager::GetReadFD(int from, int to)
{
    slogf(INFO, "#%d -> #%d\n", from + 1, to + 1);
    auto& tunnel = data->np[from][to];
    if (tunnel.status == NP_EXPIRED) {
        close(tunnel.fd[0]);
        tunnel.status = NP_EMPTY;
        tunnel.fd[0] = -1;
        tunnel.fd[1] = -1;
        memset(tunnel.path, 0, 256);
        return -1;
    }

    if (tunnel.status != NP_ESTABLISHED) {
        slogf(WARN, "This FIFO is not created\n");
        return -1;
    }

    if (0 > tunnel.fd[0]) {
        slogf(WARN, "Read FD not init\n");
        return -1;
    }

    tunnel.status = NP_READ;

    return tunnel.fd[0];
}

int NamedPipeManager::TagExpired(int self_index)
{
    for (size_t i = 0; i < USER_LIM; ++i) {
        /* I'm Write end */
        if ( data->np[self_index][i].status == NP_ESTABLISHED) {
            slogf(DEBUG, "W tag out of dated %d %lu\n", self_index, i);
            data->np[self_index][i].status = NP_EXPIRED;
        }
        /* I'm Read end */
        if ( data->np[i][self_index].status == NP_ESTABLISHED) {
            slogf(DEBUG, "R tag out of dated %lu %d\n", i, self_index);
            data->np[i][self_index].status = NP_EXPIRED;
        }

    }

    return 0;
}
