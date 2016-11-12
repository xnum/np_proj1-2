#include "NumberedPipe.h"

int NumberedPipeManager::CutToken(string& line, int& from_other, int& to_other)
{
    string newline;

    from = from_other = UNINIT; 
    to = to_other = UNINIT;
    stringstream ss(line);
    string token;
    while(ss >> token) {
        bool takeIt = false;
        if(token.size() > 1) {
            if(token[0] == '|') {
                int n = UNINIT;
                if(1 == sscanf(token.c_str(), "|%9d", &n)) {
                    add(n,OUT);
                    takeIt = true;
                }
            }

            if(token[0] == '!') {
                int n = UNINIT;
                if(1 == sscanf(token.c_str(), "!%9d", &n)) {
                    add(n,ERR);
                    takeIt = true;
                }
            }

            if(token[0] == '>') {
                int n = UNINIT;
                if(1 == sscanf(token.c_str(), ">%9d", &n)) {
                    to_other = n;
                    takeIt = true;
                }
            }

            if(token[0] == '<') {
                int n = UNINIT;
                if(1 == sscanf(token.c_str(), "<%9d", &n)) {
                    from_other = n;
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
            //printf("Close Pipe [%d]\n",it->fd[1]);
            close(it->fd[1]);
            it->fd[1] = -1;
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

            if(it->type==ERR) {
                npc.lastStderr = it->fd[1];
                npc.lastStdout = it->fd[1];
            }
            if(it->type==OUT) {
                npc.lastStdout = it->fd[1];
            }
        }
    }

    /* from,to are init when CutToken() and be set when AddNamedPipe() */
    if(from != UNINIT)
        npc.lastStderr = npc.lastStdout = to;

    if(to != UNINIT)
        npc.firstStdin = from;

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
                if(it->fd[0]!=-1)close(it->fd[0]);
                if(it->fd[1]!=-1)close(it->fd[1]);
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
        if(it->redirCount == count) {
            it->used = true;
            it->type = type;
            return;
        }
    }

    // first appear
    nps.emplace_back(NumberedPipe(count,type,true));
}

void NumberedPipeManager::AddNamedPipe(int from, int to)
{
    this->from = from;
    this->to = to;
}
