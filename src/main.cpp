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

    int fdm = posix_openpt(O_RDWR);
    if(fdm < 0) {
        printf("posix_openpt() Error:%s\n",strerror(errno));
        exit(1);
    }

    if(0 != grantpt(fdm)) {
        printf("grantpt() Error:%s\n",strerror(errno));
        exit(1);
    }

    if(0 != unlockpt(fdm)) {
        printf("unlockpt() Error:%s\n",strerror(errno));
        exit(1);
    }

    int fds = open(ptsname(fdm), O_RDWR);
    int rc = 0;

    if(fork()) { //parent

        int connfd;
        if(0 > (connfd = accept(sockfd, (struct sockaddr*)&cAddr, &len))) {
            perror("accept() error");
        }

        close(fds);

        while(1) {
            char buff[256] = {};

            struct pollfd fds[2];
            fds[0].fd = fdm;
            fds[1].fd = connfd;
            fds[0].events = POLLIN;
            fds[1].events = POLLIN;

            rc = poll(fds, 2, -1);

            if(rc == -1) {
                printf("select() Error:%s\n",strerror(errno));
                exit(1);
            }
            else if(rc > 0) {
                if(fds[0].revents & POLLIN) {
                    rc = read(fds[0].fd, buff, sizeof(buff));
                    if(rc > 0)
                        write(fds[1].fd, buff, rc);
                    else if(rc < 0) {
                        printf("read() connfd Error:%s\n",strerror(errno));
                        exit(1);
                    }
                }

                if(fds[1].revents & POLLIN) {
                    rc = read(fds[1].fd, buff, sizeof(buff));
                    if(rc > 0)
                        write(fds[0].fd, buff, rc);
                    else if(rc < 0) {
                        printf("read() connfd Error:%s\n",strerror(errno));
                        exit(1);
                    }
                }

                for(int i = 0 ; i < 2 ; ++i) {
                    if(fds[i].revents & POLLHUP || fds[i].revents & POLLERR) {
                        close(fds[0].fd);
                        close(fds[1].fd);
                        exit(0);
                    }
                }
            }
        }
        exit(1);
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
    buildConnection();

    procCtrl.SetShellPgid(getpgid(getpid()));
    procCtrl.SetupPwd();

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, backToShell);
    string line;

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
