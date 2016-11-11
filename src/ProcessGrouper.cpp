#include "ProcessGrouper.h"

int ProcessGrouper::Start(NumberedPipeConfig npc, char** envp)
{
	struct rlimit set;
	getrlimit(RLIMIT_NPROC, &set);
	int proc_limit = set.rlim_cur /2;
	//dprintf(INFO, "proc limit = %d\n", proc_limit);

	int counter = 0;
    int prev_pipe = -1;
	for( size_t i = 0 ; i < executors.size() ; ++i ) {
		Executor &exe = executors[i];
		char* const* argv = exe.cmdHnd.toArgv();

        int curr_pfd[2] = {}; // 0read(stdin) 1write(stdout)
        if(i+1 != executors.size()) {
            if(0 != pipe(curr_pfd)) {
                return -1;
            }
        }

		pid_t rc = fork();
		exe.pid = rc;

		if( rc < 0 ) { // fail
			cout << "Fork() error" << endl;
			return rc;
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

			counter++;
			if(counter >= proc_limit) {
				int status = 0;
				pid_t pid = waitpid(WAIT_ANY, &status, 0 | WUNTRACED);
				if( pid == -1 ) {
					dprintf(ERROR,"waitpid %s\n",strerror(errno));
					return errno;
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

        if( i == 0 && npc.firstStdin != UNINIT ) {
            dup2(npc.firstStdin,fileno(stdin));
        }
        if( i+1 == executors.size() && npc.lastStderr != UNINIT ) {
            dup2(npc.lastStderr,fileno(stderr));
            //dup2(npc.lastStderr,fileno(stdout));
        }
        if( i+1 == executors.size() && npc.lastStdout != UNINIT ) {
            dup2(npc.lastStdout,fileno(stdout));
        }

		if( exe.cmdHnd.redirectStdout != "" )
			freopen(exe.cmdHnd.redirectStdout.c_str(), "w+", stdout);
		if( exe.cmdHnd.redirectStdin != "" )
			freopen(exe.cmdHnd.redirectStdin.c_str(), "r", stdin);
        if( curr_pfd[1] != 0 ) {
            dup2(curr_pfd[1],1);
            close(curr_pfd[0]);
        }
        if( prev_pipe != -1 ) {
            dup2(prev_pipe,0);
        }

        return execve(argv[0],argv,envp);
	}

	return 0;
}

