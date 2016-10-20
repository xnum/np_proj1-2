#include <iostream>
#include <cstdio>
#include <cstdlib>

#include <signal.h>

#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include "Parser.h"
#include "InputHandler.h"
#include "ProcessController.h"
#include "BuiltinHelper.h"

ProcessController procCtrl;

void buildConnection()
{
    int sockfd;
    int port = 4577;

    if(0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0))) {
        perror("socket() error");
    }

    struct sockaddr_in sAddr;

    bzero((char*)&sAddr, sizeof(sAddr));
    sAddr.sin_family = AF_INET;
    sAddr.sin_addr.s_addr = INADDR_ANY;
    sAddr.sin_port = htons(port);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sAddr, sizeof(sAddr));

    if(0 > bind(sockfd, (struct sockaddr*)&sAddr, sizeof(sAddr))) {
        perror("bind() error");
    }

    if(0 > listen(sockfd, 3)) {
        perror("listen() error");
    }

    struct sockaddr_in cAddr;
    socklen_t len = sizeof(cAddr);
    bzero((char*)&cAddr, sizeof(cAddr));

    int connfd;
    if(0 > (connfd = accept(sockfd, (struct sockaddr*)&cAddr, &len))) {
        perror("accept() error");
    }

    dup2(connfd,0);
    dup2(connfd,1);
    dup2(connfd,2);
}

void waitProc()
{
	while( 1 ) {
		int status = 0;
		pid_t pid = waitpid(WAIT_ANY, &status, 0 | WUNTRACED);
		if( pid == -1 ) {
			// do until errno == ECHILD
			// means no more child 
			if(errno != ECHILD)
				printf("waitpid error: %s\n",strerror(errno));
			break;
		}

		if( WIFEXITED(status) ) {
			int rc = procCtrl.FreeProcess(pid);
			if( rc == ProcAllDone ) {
				procCtrl.TakeTerminalControl(Shell);
				return;
			}
		}
		else if( WIFSTOPPED(status) ) {
			procCtrl.TakeTerminalControl(Shell);
			return;
		}
	}
}

void backToShell(int sig __attribute__((unused))) {
	procCtrl.BackToShell();
	return;
}

int main()
{
	procCtrl.SetShellPgid(getpgid(getpid()));
    procCtrl.SetupPwd();
	
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, backToShell);
	string line;

    buildConnection();

    puts("****************************************");
    puts("** Welcome to the information server. **");
    puts("****************************************");
    printf("* Your Directory: %s\n",procCtrl.pwd.c_str());

	InputHandler InHnd;
	while( 1 ) {
        procCtrl.npManager.Free();
		int fg=0;
		cout << "% " << flush;
		line = InHnd.Getline();
		if( line == "" ) {
			//procCtrl.TakeTerminalControl(Shell);
			//procCtrl.RefreshJobStatus();
			//printf("\b\b  \b\b");
			continue;
		}
		else if( BuiltinHelper::IsSupportCmd(line) ) {
            procCtrl.npManager.Count();
			if( Wait != BuiltinHelper::RunBuiltinCmd(line) )
                continue;
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
                continue;

            bool cmd404 = false;
            vector<Executor> exes;
            for( auto& cmd : cmds ) {
                string res = procCtrl.ToPathname(cmd.name);
                if(res == "") {
                    dprintf(1,"Unknown command: [%s].\n",cmd.name.c_str());
                    cmd404 = true;
                }
                cmd.name = res;
                exes.emplace_back(Executor(cmd));
            }

            if(cmd404)continue;

            procCtrl.AddProcGroups(exes, line);
            if( Failure == procCtrl.StartProc(fg==0 ? true : false) ) {
                continue;
            }
		}

		if(fg == 0)waitProc();
	}

	puts("dead");
	return 0;
}
