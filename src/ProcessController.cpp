#include "ProcessController.h"

int ProcessController::AddProcGroups(const vector<Executor>& exes, const string& cmd)
{
	pgrps.emplace_back(ProcessGrouper(exes));
	ProcessGrouper &pgrp = *pgrps.rbegin();
	pgrp.originCmds = cmd;
	return 0;
}

int ProcessController::StartProc(bool isfg)
{
	if( pgrps.size() == 0 ) {
		dprintf(ERROR,"No processes could be start\n");
		exit(1);
	}

	ProcessGrouper &pgrp = *pgrps.rbegin();
    int rc = 0;
	if( (rc = pgrp.Start(npManager.TakeConfig(), envManager.ToEnvp())) != 0 ) {
        dprintf(ERROR,"execve() %s\n",strerror(errno));
		pgrp.PassSignal(SIGKILL);
        return Failure;
	}

	fgIndex = pgrps.size() -1;
	if(isfg) {
		TakeTerminalControl(pgrp.GetPgid());
	}

	return Success;
}

int ProcessController::TakeTerminalControl(pid_t pgid)
{
    if(!godmode)
        return 0;

	pid_t target = pgid;
	if( pgid == Shell ) {
		target = shellPgid;
	}

	if( pgid == ForeGround ) {
		if( fgIndex != Shell )
			target = pgrps[fgIndex].GetPgid();
		else
			target = shellPgid;
	}

	fflush(stdout);

	bool hasError = false;
	if( 0 != tcsetpgrp(0, target) ) {
        dprintf(WARN,"setpgrp() %s\n",strerror(errno));
		hasError = true;
	}
	if( 0 != tcsetpgrp(1, target) ) {
        dprintf(WARN,"setpgrp() %s\n",strerror(errno));
		hasError = true;
	}
	if( 0 != tcsetpgrp(2, target) ) {
        dprintf(WARN,"setpgrp() %s\n",strerror(errno));
		hasError = true;
	}

	if( hasError == true ) {
		if( pgid == getpgid(0) || pgid == Shell ) {
			dprintf(ERROR,"same pgid but set pgrp failed\n");
			exit(1);
		}
		else
			TakeTerminalControl(Shell);
	}

	return 0;
}

int ProcessController::FreeProcess(pid_t pid) 
{
	for( size_t i = 0 ; i < pgrps.size() ; ++i ) {
		int rc = pgrps[i].NotifyTerminated(pid);
		if( rc != ProcNotMine ) {
			if( rc == ProcAllDone ) {
				pgrps.erase(pgrps.begin() + i);
				if( fgIndex == (int)i ) {
					fgIndex = pgrps.size()-1;
					return ProcAllDone;
				}
			}
			break;
		}
	}
	return ProcNotAllDone;
}

int ProcessController::SendSignalToFG(int sig) 
{
	kill(pgrps[fgIndex].GetPgid(),sig);
	return 0;
}

void ProcessController::BackToShell()
{
	TakeTerminalControl(Shell);
	SendSignalToFG(SIGTSTP);
}

int ProcessController::BringToFront(int index)
{
	if( index == -1 ) {
		TakeTerminalControl(ForeGround);
		SendSignalToFG(SIGCONT);
	}
	else if( 0 <= index && index < (int)pgrps.size() ) {
		fgIndex = index;
		TakeTerminalControl(ForeGround);
		SendSignalToFG(SIGCONT);
	}
	else {
		dprintf(WARN,"index out of range\n");
		return Failure;
	}

	return Success;
}

int ProcessController::BringToBack(int index)
{
	if( index == -1 ) {
		SendSignalToFG(SIGCONT);
	}
	else if( 0 <= index && index < (int)pgrps.size() ) {
		fgIndex = index;
		SendSignalToFG(SIGCONT);
	}
	else {
		dprintf(WARN,"index out of range\n");
		return Failure;
	}

	return Success;
}

void ProcessController::printJobs()
{
	for( size_t i = 0 ; i < pgrps.size() ; ++i ) {
		printf("\n=== Process Group %lu ===\n",i);
		if(fgIndex == (int)i)puts("Foreground Process Group");
		else				 puts("Background Process Group");
		printf("Cmd: %s\n",pgrps[i].originCmds.c_str());
		printf("pgid: %d\n",pgrps[i].GetPgid());
	}
}

void ProcessController::RefreshJobStatus()
{
	bool retry = false;
	do {
		retry = false;
		for( size_t i = 0 ; i < pgrps.size() ; ++i ) {
			if( 0 != kill(- pgrps[i].GetPgid(), 0) ) {
				pgrps.erase(pgrps.begin() +i);
				retry = true;
				break;
			}
		}	
	} while(retry);
}

string ProcessController::ToPathname(string filename)
{
    string paths = envManager.getenv("PATH");
    replace(paths.begin(), paths.end(), ':', ' ');
    stringstream ss(paths);
    string path;
    while(ss >> path) {
        string pathname = pwd + '/' + path + '/' + filename;
        if(0 == access(pathname.c_str(), X_OK))
            return pathname;
    }
    return "";
}

void ProcessController::SetupPwd()
{
    char buff[1024] = {};
    if(NULL == getcwd(buff, 1024))
        perror("get pwd error");
    pwd = string(buff);
}





