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
#include <sys/ioctl.h>

#include "Logger.h"
#include "Parser.h"
#include "InputHandler.h"
#include "ProcessController.h"
#include "BuiltinHelper.h"

ProcessController procCtrl;

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

void serveForever(int sockfd)
{
    int rc = 0;

    while(1) {
WAIT_CONN:
        dprintf(DEBUG, "start waiting connection\n");
        struct sockaddr_in cAddr;
        socklen_t len = sizeof(cAddr);
        bzero((char*)&cAddr, sizeof(cAddr));

        int connfd;
        if(0 > (connfd = accept(sockfd, (struct sockaddr*)&cAddr, &len))) {
            dprintf(ERROR, "accept() %s\n", strerror(errno));
        }

        dprintf(DEBUG, "connection established\n");

        int fdm = posix_openpt(O_RDWR);
        if(fdm < 0) {
            dprintf(ERROR, "posix_openpt() %s\n", strerror(errno));
            exit(1);
        }

        if(0 != grantpt(fdm)) {
            dprintf(ERROR, "grantpt() %s\n", strerror(errno));
            exit(1);
        }

        if(0 != unlockpt(fdm)) {
            dprintf(ERROR, "unlockpt() %s\n", strerror(errno));
            exit(1);
        }

        int fds = open(ptsname(fdm), O_RDWR);
        if(fds == -1)
            dprintf(ERROR, "open(fds) %s\n", strerror(errno));

        int cpid = 0;
        if(cpid = fork()) { //parent

            close(fds);

            dprintf(DEBUG, "start serving connection\n");
            while(1) {
                char buff[256] = {};

                struct pollfd fds[2];
                fds[0].fd = fdm;
                fds[1].fd = connfd;
                fds[0].events = POLLIN;
                fds[1].events = POLLIN;

                rc = poll(fds, 2, -1);

                if(rc == -1) {
                    dprintf(ERROR, "poll() %s\n",strerror(errno));
                    exit(1);
                }
                else if(rc > 0) {
                    if(fds[0].revents & POLLIN) {
                        rc = read(fds[0].fd, buff, sizeof(buff));
                        if(rc > 0)
                            write(fds[1].fd, buff, rc);
                        else if(rc < 0) {
                            dprintf(ERROR,"read() fdm %s\n",strerror(errno));
                            exit(1);
                        }
                    }

                    if(fds[1].revents & POLLIN) {
                        rc = read(fds[1].fd, buff, sizeof(buff));
                        if(rc > 0)
                            write(fds[0].fd, buff, rc);
                        else if(rc < 0) {
                            dprintf(ERROR,"read() connfd Error:%s\n",strerror(errno));
                            exit(1);
                        }
                    }

                    /* connfd or fdm exited */
                    for(int i = 0 ; i < 2 ; ++i) {
                        if(fds[i].revents & POLLHUP || fds[i].revents & POLLERR) {
                            dprintf(WARN,"disconnect\n");
                            close(fds[0].fd);
                            close(fds[1].fd);

                            dprintf(INFO,"wait child:%d\n",cpid);
                            int status = 0;
                            pid_t pid = waitpid(cpid, &status, 0);

                            goto WAIT_CONN;
                        }
                    }
                }
            }
        }
        else { // child
            struct termios slave_orig_term_settings; // Saved terminal settings
            struct termios new_term_settings; // Current terminal settings

            // CHILD

            // Close the master side of the PTY
            close(fdm);
            close(sockfd);

            // Save the defaults parameters of the slave side of the PTY
            rc = tcgetattr(fds, &slave_orig_term_settings);

            // Set RAW mode on slave side of PTY
            new_term_settings = slave_orig_term_settings;
            cfmakeraw (&new_term_settings);
            tcsetattr (fds, TCSANOW, &new_term_settings);

            // The slave side of the PTY becomes the standard input and outputs of the child process
            close(0); // Close standard input (current terminal)
            close(1); // Close standard output (current terminal)
            close(2); // Close standard error (current terminal)

            dup(fds); // PTY becomes standard input (0)
            dup(fds); // PTY becomes standard output (1)
            dup(fds); // PTY becomes standard error (2)

            // Now the original file descriptor is useless
            close(fds);

            // Make the current process a new session leader
            setsid();

            // As the child is a session leader, set the controlling terminal to be the slave side of the PTY
            // (Mandatory for programs like the shell to make them manage correctly their outputs)
            ioctl(0, TIOCSCTTY, 1);
            return;
        }
    }
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
                dprintf(ERROR,"waitpid %s\n",strerror(errno));
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
    int sockfd = buildConnection();
    serveForever(sockfd);

    procCtrl.SetShellPgid(getpgid(getpid()));
    procCtrl.SetupPwd();

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, backToShell);
    string line;

    puts("****************************************");
    puts("** Welcome to the information server. **");
    puts("****************************************");
    printf("* Your Directory: %s *\n",procCtrl.pwd.c_str());

    InputHandler InHnd;
    while( 1 ) {
        procCtrl.npManager.Free();
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
    }

    puts("dead");
    return 0;
}
