#include "NumberedPipe.h"

int NumberedPipeManager::CutNumberedPipeToken(string& line)
{
    string newline;

    stringstream ss(line);
    string token;
    while(ss >> token) {
        bool takeIt = false;
        if(token.size() > 1) {
            if(token[0] == '|') {
                int n = -1;
                if(1 == sscanf(token.c_str(), "|%d", &n)) {
                    add(n,OUT);
                    takeIt = true;
                }
            }

            if(token[0] == '!') {
                int n = -1;
                if(1 == sscanf(token.c_str(), "!%d", &n)) {
                    add(n,ERR);
                    takeIt = true;
                }
            }
        }

        if(!takeIt)
            newline += " " + token + " ";
    }

    line = newline;

    return 0;
}

NumberedPipeConfig NumberedPipeManager::TakeConfig()
{
    NumberedPipeConfig npc;
    npc.firstStdin = UNINIT;
    npc.lastStdout = UNINIT;
    npc.lastStderr = UNINIT;

    for(auto it = nps.begin() ; it != nps.end() ; ++it) {
        if(it->redirCount-- == 0) {
            close(it->fd[1]);
            npc.firstStdin = it->fd[0];
        }
        if(it->used) {
            it->used = false;
            if(it->fd[0] == UNINIT && it->fd[1] == UNINIT) {
                pipe(it->fd);
                //printf("Create Pipe [%d,%d]\n",it->fd[0],it->fd[1]);
            }
            else {
                //printf("Use Exist Pipe [%d,%d]\n",it->fd[0],it->fd[1]);
            }

            if(it->type==ERR)
                npc.lastStderr = it->fd[1];
            if(it->type==OUT)
                npc.lastStdout = it->fd[1];
        }
    }

    return npc;
}

void NumberedPipeManager::Count()
{
    for(auto it = nps.begin() ; it != nps.end() ; ++it) {
        it->redirCount--;
    }
}


void NumberedPipeManager::Free()
{
    bool done = true;
    do {
        done = true;
        for(auto it = nps.begin() ; it != nps.end() ; ++it) {
            if(it->redirCount<0) {
                //printf("Close Pipe [%d,%d]\n",it->fd[0],it->fd[1]);
                close(it->fd[0]);
                close(it->fd[1]);
                nps.erase(it);
                done = false;
                break;
            }
        }
    } while(!done);
}

void NumberedPipeManager::add(int count,NPType type)
{
    for(auto it = nps.begin() ; it != nps.end() ; ++it) {
        if(it->redirCount == count && it->type == type) {
            it->used = true;
            return;
        }
    }

    nps.emplace_back(NumberedPipe(count,type,true));
}
