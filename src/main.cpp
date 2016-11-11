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


int waitProc()
{
    while( 1 ) {
        int status = 0;
        pid_t pid = waitpid(WAIT_ANY, &status, 0);
        if( pid == -1 ) {
            // do until errno == ECHILD
            // means no more child 
            if(errno != ECHILD)
                dprintf(ERROR,"waitpid %s\n",strerror(errno));
            return 0;
        }
    }
}

int serve(ProcessController& procCtrl, string line)
{
    procCtrl.npManager.Free();
    int fg=0;
    if( line == "" ) {
        return 0;
    }
    else if( BuiltinHelper::IsSupportCmd(line) ) {
        procCtrl.npManager.Count();
        return BuiltinHelper::RunBuiltinCmd(procCtrl, line);
    }
    else {
        procCtrl.npManager.CutNumberedPipeToken(line);
        vector<Command> cmds;
        if( !Parser::IsExpandable(line) ) {
            cmds = Parser::Parse(line,fg);
        }
        else {
            cmds = Parser::ParseGlob(line,fg);
        }

        if(cmds.size() == 0)
            return 0;

        bool cmd404 = false;
        vector<Executor> exes;
        for( auto& cmd : cmds ) {
            cmd.filename = cmd.name;
            string res = procCtrl.ToPathname(cmd.name);
            if(res == "") {
                fprintf(stderr,"Unknown command: [%s].\n",cmd.name.c_str());
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

        procCtrl.AddProcGroups(exes, line);
        int rc = procCtrl.StartProc();
        if( rc == Failure ) {
            return 0;
        }
    }

    // if StartProc got Success
    if(fg == 0)waitProc();

    return 0;
}

int main()
{
    //serveForever(sockfd);

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    map<int,ProcessController> procCtrls;

    TCPServer tcpServ;
    tcpServ.Init(4577);
    string line;
    int connfd;
    while(T_Success == tcpServ.GetRequest(line, connfd))
    {
        dprintf(DEBUG, "serving %d\n",connfd);

        int stdin_cp = dup(0);
        int stdout_cp = dup(1);
        int stderr_cp = dup(2);

        dup2(connfd, 0);
        dup2(connfd, 1);
        dup2(connfd, 2);

        if(Exit == serve(procCtrls[connfd], line)) {
            close(connfd);
        } else {
            write(connfd, "% ", 2);
        }

        dup2(stdin_cp, 0);
        dup2(stdout_cp, 1);
        dup2(stderr_cp, 2);
        
        close(stdin_cp);
        close(stdout_cp);
        close(stderr_cp);

        dprintf(DEBUG, "served %d\n",connfd);
    }

    puts("dead");
    return 0;
}
