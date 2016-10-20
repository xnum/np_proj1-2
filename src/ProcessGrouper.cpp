#include "ProcessGrouper.h"

int ProcessGrouper::Start(NumberedPipeConfig npc)
{
	for( size_t i = 0 ; i < executors.size()-1 ; ++i ) {
		executors[i].PipeWith(executors[i+1]);
	}

    printf("%d %d %d\n",npc.firstStdin,npc.lastStdout,npc.lastStderr);

	for( size_t i = 0 ; i < executors.size() ; ++i ) {
		Executor &exe = executors[i];
		char* const* argv = exe.cmdHnd.toArgv();

		pid_t rc = fork();
		exe.pid = rc;


		if( rc < 0 ) { // fail
			cout << "Fork() error" << endl;
			return rc;
		}

		if( rc > 0 ) { // parent
			if( i == 0 )
				pgid = exe.pid;
			setpgid(executors[i].pid,pgid);
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
            printf("Take Stdin With %d\n",npc.firstStdin);
            dup2(npc.firstStdin,fileno(stdin));
        }
        if( i+1 == executors.size() && npc.lastStderr != UNINIT ) {
            printf("Take Stderr With %d\n",npc.lastStderr);
            dup2(npc.lastStderr,fileno(stderr));
        }
        if( i+1 == executors.size() && npc.lastStdout != UNINIT ) {
            printf("Take Stdout With %d\n",npc.lastStdout);
            dup2(npc.lastStdout,fileno(stdout));
        }

		if( exe.cmdHnd.redirectStdout != "" )
			freopen(exe.cmdHnd.redirectStdout.c_str(), "w+", stdout);
		if( exe.cmdHnd.redirectStdin != "" )
			freopen(exe.cmdHnd.redirectStdin.c_str(), "r", stdin);
		if( exe.fd[0][0] != -1 ) {
			dup2(exe.fd[0][0],0);
		}
		if( exe.fd[1][1] != -1 ) {
			dup2(exe.fd[1][1],1);
		}
		xnsh::CloseAllPipe(executors);
		int ret = execvp(argv[0],argv);
        dprintf(1,"Unknown command: [%s].\n",argv[0]);
        return ret;
	}

	xnsh::CloseAllPipe(executors);

	return 0;
}

int ProcessGrouper::NotifyTerminated(pid_t pid) {
	bool allDone = true;
	bool notMine = true;
	for( size_t i = 0 ; i < executors.size() ; ++i ) {
		if( executors[i].pid == pid ) {
			notMine = false;
			executors[i].done = true;
		}
		if( executors[i].done == false ) {
			allDone = false;
		}
	}

	if( notMine == true )
		return ProcNotMine;

	if( allDone == true )
		return ProcAllDone;

	return ProcNotAllDone;

}

int ProcessGrouper::PassSignal(int sig) {
	if(executors.size() == 0) {
		printf("Error:Process group doesn't contain any process\n");
		exit(1);
	}

	// negtive for process group
	kill(-(executors[0].pid),sig);
	return 0;
}

pid_t ProcessGrouper::GetPgid() {
	if(executors.size() == 0) {
		printf("Error:Process group doesn't contain any process\n");
		exit(1);
	}

	return pgid;
}
