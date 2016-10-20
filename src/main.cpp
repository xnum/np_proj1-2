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
#include "EnvironManager.h"
#include "NumberedPipe.h"

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
	
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, backToShell);
	string line;

    buildConnection();

    puts("****************************************");
    puts("** Welcome to the information server. **");
    puts("****************************************");

    EnvironManager envManager;
    NumberedPipeManager npManager;

	InputHandler InHnd;
	while( 1 ) {
		int fg=0;
		cout << "% " << flush;
		line = InHnd.Getline();
		if( line == "" ) {
			procCtrl.TakeTerminalControl(Shell);
			procCtrl.RefreshJobStatus();
			printf("\b\b  \b\b");
			continue;
		}
		else if( BuiltinHelper::IsSupportCmd(line) ) {
			if( Wait != BuiltinHelper::RunBuiltinCmd(line, envManager) )
                continue;
		}
		else {
            npManager.CutNumberedPipeToken(line);
            cout << "Command = " << line << endl;
            continue;
            if( !Parser::IsExpandable(line) ) {
                auto cmds = Parser::Parse(line,fg);

                vector<Executor> exes;
                for( const auto& cmd : cmds )
                    exes.emplace_back(Executor(cmd));

                procCtrl.AddProcGroups(exes, line);
                if( Failure == procCtrl.StartProc(fg==0 ? true : false) ) {
                    continue;
                }
            }
            else {
                auto cmds = Parser::ParseGlob(line,fg);

                if(cmds.size() == 0)
                    continue;

                vector<Executor> exes;
                for( const auto& cmd : cmds ) {
                    exes.emplace_back(Executor(cmd));
                }

                procCtrl.AddProcGroups(exes, line);
                if( Failure == procCtrl.StartProc(fg==0 ? true : false) ) {
                    continue;
                }
            }
		}

		if(fg == 0)waitProc();
	}

	puts("dead");
	return 0;
}
