#include "NamedPipe.h"

NamedPipeManager::NamedPipeManager()
{
    for(size_t i = 0; i < USER_LIM; ++i) {
        for(size_t j = 0; j < USER_LIM; ++j) {
            np[i][j].status = NP_EMPTY;
            np[i][j].fd[0] = np[i][j].fd[1] = -1;
        }
    }
}

int NamedPipeManager::BuildPipe(int from, int to)
{
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
