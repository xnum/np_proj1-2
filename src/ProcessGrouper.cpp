#include "ProcessGrouper.h"

int ProcessGrouper::Start(int connfd, NumberedPipeConfig npc, char** envp)
{
	struct rlimit set;
	getrlimit(RLIMIT_NPROC, &set);
	int proc_limit = set.rlim_cur /2;
	//slogf(INFO, "proc limit = %d\n", proc_limit);

    /* if not specific, connfd is default file */
    int firstStdin = connfd;
    int lastStderr = connfd;
    int lastStdout = connfd;

    if(npc.firstStdin != UNINIT)
        firstStdin = npc.firstStdin;
    if(npc.lastStderr != UNINIT)
        lastStderr = npc.lastStderr;
    if(npc.lastStdout != UNINIT)
        lastStdout = npc.lastStdout;

	int counter = 0;
    int prev_pipe = -1;
	for( size_t i = 0 ; i < executors.size() ; ++i ) {
		Executor &exe = executors[i];
		char* const* argv = exe.cmdHnd.toArgv();

        int curr_pfd[2] = {}; // 0read(stdin) 1write(stdout)
        if(i+1 != executors.size()) {
            if(0 != pipe(curr_pfd)) {
                slogf(WARN, "pipe() error %s\n",strerror(errno));
                return Wait;
            }
        }

		pid_t rc = fork();
		exe.pid = rc;

		if( rc < 0 ) { // fail
            slogf(WARN, "fork() error %s\n",strerror(errno));
			return Wait;
		}

		if( rc > 0 ) { // parent
            if(prev_pipe!=-1) {
                close(prev_pipe);
            }
            if(curr_pfd[1]!=0)
            {
                prev_pipe = curr_pfd[0];
                close(curr_pfd[1]);
            }

            /* if spawned child reached limit, wait for anyone dead */
			counter++;
			if(counter >= proc_limit) {
				int status = 0;
				pid_t pid = waitpid(WAIT_ANY, &status, 0 | WUNTRACED);
				if( pid == -1 ) {
					slogf(WARN,"waitpid %s\n",strerror(errno));
					return Fail;
				}
				else {
					counter--;
				}
			}

			continue;
		}

		//child
		signal(SIGTTOU, SIG_DFL);
		signal(SIGTTIN, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);
		signal(SIGCONT, SIG_DFL);

        /* single command redirect request */
		if( exe.cmdHnd.redirectStdout != "" )
			freopen(exe.cmdHnd.redirectStdout.c_str(), "w+", stdout);
		if( exe.cmdHnd.redirectStdin != "" )
			freopen(exe.cmdHnd.redirectStdin.c_str(), "r", stdin);

        /* pipe from previos command */
        if( curr_pfd[1] != 0 ) {
            dup2(curr_pfd[1],1);
            close(curr_pfd[0]);
        }
        if( prev_pipe != -1 ) {
            dup2(prev_pipe,0);
        }

        /* for last and first command */
        if( i == 0 && firstStdin != UNINIT ) {
            slogf(INFO, "firstStdin = %d\n",firstStdin);
            if(0 > dup2(firstStdin,0))
                slogf(INFO, "%s\n",strerror(errno));
        }

        if( i+1 == executors.size() && lastStdout != UNINIT ) {
            slogf(INFO, "lastStdout = %d\n",lastStdout);
            if(0 > dup2(lastStdout,1))
                slogf(INFO, "%s\n",strerror(errno));
        }

        if( i+1 == executors.size() && lastStderr != UNINIT ) {
            slogf(INFO, "lastStderr = %d\n",lastStderr);
            if(0 > dup2(lastStderr,2))
                slogf(INFO, "%s\n",strerror(errno));
        }

        if(firstStdin!=UNINIT) {
            close(firstStdin);
        }

        return execve(argv[0],argv,envp);
	}

	return Ok;
}

