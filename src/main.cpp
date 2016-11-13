#include <iostream>
#include <cstdio>
#include <cstdlib>

#include <signal.h>

#include "TCPServer.h"
#include "Logger.h"
#include "Parser.h"
#include "InputHandler.h"
#include "ProcessController.h"
#include "BuiltinHelper.h"
#include "NamedPipe.h"
#include "Message.h"

NamedPipeManager fifoMan;
MessageCenter msgCenter;

int waitProc()
{
    while( 1 ) {
        int status = 0;
        pid_t pid = waitpid(WAIT_ANY, &status, 0);
        if( pid == -1 ) {
            // do until errno == ECHILD
            // means no more child 
            if(errno != ECHILD)
                slogf(ERROR,"waitpid %s\n",strerror(errno));
            return 0;
        }
    }
}

int serve(ProcessController& procCtrl, string line)
{
    /* remove used number pipe */
    procCtrl.npManager.Free();
    if( line == "" ) {
        return 0;
    }

    if( BuiltinHelper::IsSupportCmd(line) ) {
        procCtrl.npManager.Count();
        /* Exit may return here */
        return BuiltinHelper::RunBuiltinCmd(procCtrl, line);
    }

    string originLine = line;
    for(int i = 0 ; i < originLine.size(); ++i) {
        if(originLine[i] == '\r' || originLine[i] == '\n') {
            originLine[i] = '\0';
        } 
    }
    int from, to; // raw number
    procCtrl.npManager.CutToken(line, from, to);
    vector<Command> cmds;
    if( !Parser::IsExpandable(line) ) {
        cmds = Parser::Parse(line);
    }
    else {
        cmds = Parser::ParseGlob(line);
    }

    if(cmds.size() == 0)
        return 0;

    bool cmd404 = false;
    vector<Executor> exes;
    for( auto& cmd : cmds ) {
        cmd.filename = cmd.name;
        string res = procCtrl.ToPathname(cmd.name);
        if(res == "") {
            dprintf(procCtrl.connfd,"Unknown command: [%s].\n",cmd.name.c_str());
            cmd404 = true;
            break;
        }
        cmd.name = res;
        exes.emplace_back(Executor(cmd));
    }

    if(cmd404) {
        procCtrl.npManager.Count();
        return 0;
    }

    /* we want send something to somebody*/
    int to_pipe_fd = UNINIT, from_pipe_fd = UNINIT;
    int self_index = msgCenter.getIndexByConnfd(procCtrl.connfd);
    if(to != UNINIT) {
        msgCenter.CreatedPipe(self_index, to-1, originLine.c_str());
        if(0 > fifoMan.BuildPipe(self_index, to-1)) {
            msgCenter.PipeExist(self_index, to-1);
            //dprintf(procCtrl.connfd,"Build Pipe Failed (Exists?) .\n");
            return 0;
        }

        int fd = 0;
        if(0 > (fd = fifoMan.GetWriteFD(self_index, to-1))) {
            dprintf(procCtrl.connfd,"Get Write End Failed (WTF?) .\n");
            return 0;
        }

        to_pipe_fd = fd;
    }

    /* since we are the received side, pipe should be build and connected */
    /* only need to get read fd                             */
    if(from != UNINIT) {
        int fd = 0;
        msgCenter.ReceivePipe(from-1, self_index, originLine.c_str());
        if(0 > (fd = fifoMan.GetReadFD(from-1, self_index))) {
            msgCenter.PipeNotExist(from-1, self_index);
            //dprintf(procCtrl.connfd,"Get Read End Failed (Not Exists?) .\n");
            return 0;
        }

        from_pipe_fd = fd;
    }

    procCtrl.npManager.AddNamedPipe(from_pipe_fd, to_pipe_fd);

    procCtrl.AddProcGroups(exes, originLine);
    int rc = procCtrl.StartProc();

    if(to_pipe_fd!=UNINIT)    close(to_pipe_fd);

    slogf(INFO, "rc = %d\n", rc);
    if(rc == Wait || rc == Ok)
        waitProc();

    /* task done, clean up fd we just got and used */
    fifoMan.Free();

    return 0;
}

int main()
{
    map<int,ProcessController> procCtrls;

    TCPServer tcpServ;
    tcpServ.Init(4577);
    string line;
    int connfd;
    while(T_Success == tcpServ.GetRequest(line /* command */, connfd /* return corresponding socket id */))
    {
        msgCenter.UpdateFromTCPServer(tcpServ.client_info);
        //slogf(DEBUG, "serving %d\n",connfd);

        procCtrls[connfd].connfd = connfd;
        if(Exit == serve(procCtrls[connfd], line)) {
            msgCenter.UserLeft(connfd);
            tcpServ.RemoveUser(connfd);
            close(connfd);
        } else {
            write(connfd, "% ", 2);
        }

        //slogf(DEBUG, "served %d\n",connfd);
    }

    puts("dead");
    return 0;
}
