#include <iostream>
#include <cstdio>
#include <cstdlib>

#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>

#include "Logger.h"
#include "Parser.h"
#include "InputHandler.h"
#include "ProcessController.h"
#include "BuiltinHelper.h"
#include "Message.h"

ProcessController procCtrl;
MessageCenter msgCenter;

void sig(int signo)
{
    if(signo == SIGUSR1)
    {
        msgCenter.OpenReadEnd();
    }
}

int waitProc(bool waitAll = true)
{
	int num = 0;
    while( 1 ) {
        int status = 0;
        pid_t pid = waitpid(WAIT_ANY, &status, 0 | WUNTRACED);
        if( pid == -1 ) {
            // do until errno == ECHILD
            // means no more child 
            if(errno != ECHILD)
                dprintf(ERROR,"waitpid %s\n",strerror(errno));
            break;
        }
		else {
			num++;
		}

        if( WIFEXITED(status) ) {
            int rc = procCtrl.FreeProcess(pid);
            if( rc == ProcAllDone ) {
                procCtrl.TakeTerminalControl(Shell);
                return num;
            }
        }
        else if( WIFSTOPPED(status) ) {
            procCtrl.TakeTerminalControl(Shell);
            return num;
        }

		if(!waitAll && num != 0)
			return num;
    }

	return num;
}


int buildConnection()
{
    int sockfd;
    int port = 4577;

    if(0 > (sockfd = socket(AF_INET, SOCK_STREAM, 0))) {
        dprintf(ERROR, "socket() %s\n", strerror(errno));
    }

    struct sockaddr_in sAddr;

    bzero((char*)&sAddr, sizeof(sAddr));
    sAddr.sin_family = AF_INET;
    sAddr.sin_addr.s_addr = INADDR_ANY;
    sAddr.sin_port = htons(port);

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sAddr, sizeof(sAddr));

    if(0 > bind(sockfd, (struct sockaddr*)&sAddr, sizeof(sAddr))) {
        dprintf(ERROR, "bind() %s\n", strerror(errno));
    }

    if(0 > listen(sockfd, 3)) {
        dprintf(ERROR, "listen() %s\n", strerror(errno));
    }

    dprintf(DEBUG, "success: return sockfd %d\n", sockfd);

    return sockfd;
}

struct Client {
    int id;
    int connfd;
    pid_t pid;
    Client(int c,int a,pid_t b) : id(c),connfd(a),pid(b) {}
};

void serveForever(int sockfd)
{
    int rc = 0;
    vector<Client> clients;
    pollfd pfd[1] = { { .fd = sockfd, .events = POLLIN } };

    while(1) {
        rc = poll(pfd, 1, 100);

        if(rc < 0) {
            dprintf(ERROR, "poll() %s\n", strerror(errno));
        }
        if(rc == 0) { /* timeout */
            /* caught defunct */
            while(1) {
                int status = 0;
                pid_t pid = waitpid(0, &status, WNOHANG);
                if(pid <= 0) {
                    //dprintf(DEBUG,"waitpid %s\n",strerror(errno));
                    break;
                }
                auto iter = clients.begin();
                for(; iter != clients.end() ; ++iter) {
                    if( iter->pid == pid )
                        break;
                }

                if( iter == clients.end() ) {
                    dprintf(WARN, "not matched pid\n");
                } else {
                    close(iter->connfd);
                    clients.erase(iter);
                }
            }

            /* broadcast */
            char msg[2048] = {};
            if(msgCenter.GetBroadCast(msg,2048)) {
                for(int i = 0 ; i < clients.size() ; ++i) {
                    write(clients[i].connfd, msg, strlen(msg));
                }
            }

            /* send */
            for(int i = 0 ; i < clients.size() ; ++i) {
                for(int j = 0 ; j < clients.size() ; ++j) {
                    msgCenter.Trans(clients[i].id, clients[j].id, clients[j].connfd);
                }
            }

            /* notify named pipe */
            for(int i = 0 ; i < clients.size() ; ++i) {
                for(int j = 0 ; j < clients.size() ; ++j) {
                    int r = msgCenter.Notify(clients[i].id, clients[j].id);
                    if(r == NP_WAIT) {
                        kill(clients[j].pid,SIGUSR1);
                    }
                }
            }
        }
        if(rc > 0) {
            dprintf(DEBUG, "start waiting connection\n");
            struct sockaddr_in cAddr;
            socklen_t len = sizeof(cAddr);
            bzero((char*)&cAddr, sizeof(cAddr));

            int connfd;
            if(0 > (connfd = accept(sockfd, (struct sockaddr*)&cAddr, &len))) {
                dprintf(ERROR, "accept() %s\n", strerror(errno));
            }

            dprintf(DEBUG, "connection established\n");
            char name[] = "(No Name)";
            char *ip = inet_ntoa(cAddr.sin_addr);
            char ipp[128] = {};
            sprintf(ipp,"%s/%d",ip,ntohs(cAddr.sin_port));

            int id = msgCenter.AddUser(connfd, name, ipp);

            pid_t cpid = 0;
            if(0 != (cpid = fork())) {
                //close(connfd);
                clients.emplace_back(Client(id,connfd, cpid));
                continue;
            }
            else {
                msgCenter.self_index = id;
                dup2(connfd, 0);
                dup2(connfd, 1);
                dup2(connfd, 2);
                for(int i = 0 ; i < clients.size() ; ++i)
                    close(clients[i].connfd);
                close(connfd);
                return;
            }
        }
    }
}

int main()
{
    int sockfd = buildConnection();
    serveForever(sockfd);

    procCtrl.SetShellPgid(getpgid(getpid()));
    procCtrl.SetupPwd();

    //signal(SIGCHLD, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGUSR1, sig);
    string line;

    puts("****************************************");
    puts("** Welcome to the information server. **");
    puts("****************************************");

    InputHandler InHnd;
    while( 1 ) {
        procCtrl.npManager.Free();
        int fg=0;
        cout << "% " << flush;
        line = InHnd.Getline();
        if( line == "" ) {
            continue;
        }
        else if( BuiltinHelper::IsSupportCmd(line) ) {
            procCtrl.npManager.Count();
            if( Wait != BuiltinHelper::RunBuiltinCmd(line) )
                continue;
        }
        else {
            string oldline = line;
            for(int i = 0 ; i < oldline.size(); ++i) {
               if(oldline[i] == '\r' || oldline[i] == '\n') {
                   oldline[i] = '\0';
               } 
            }
            procCtrl.npManager.CutNumberedPipeToken(line);
            if(procCtrl.npManager.fifo_write != UNINIT) {
                msgCenter.MentionNamedPipe(procCtrl.npManager.fifo_write, oldline.c_str()); 
            }
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
                cmd.filename = cmd.name;
                string res = procCtrl.ToPathname(cmd.name);
                if(!godmode) {
                    if(res == "") {
                        fprintf(stderr,"Unknown command: [%s].\n",cmd.name.c_str());
                        cmd404 = true;
                        break;
                    }
                    cmd.name = res;
                }
                exes.emplace_back(Executor(cmd));
            }

            if(cmd404) {
                procCtrl.npManager.Count();
                continue;
            }

            procCtrl.AddProcGroups(exes, line);
            int rc = procCtrl.StartProc(fg==0 ? true : false);
            if( rc == Failure ) {
                continue;
            }
        }

        // if StartProc got Success
        if(fg == 0)waitProc();
        if(procCtrl.npManager.fifo_read != UNINIT) {
            msgCenter.FreeNamedPipe(msgCenter.self_index);
        }
    }

    puts("dead");
    return 0;
}
