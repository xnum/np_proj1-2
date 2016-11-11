#include "ProcessController.h"

ProcessController::ProcessController()
{
    SetupPwd();
}

int ProcessController::AddProcGroups(const vector<Executor>& exes, const string& cmd)
{
	pgrps.emplace_back(ProcessGrouper(exes));
	ProcessGrouper &pgrp = *pgrps.rbegin();
	pgrp.originCmds = cmd;
	return 0;
}

int ProcessController::StartProc()
{
	if( pgrps.size() == 0 ) {
		dprintf(ERROR,"No processes could be start\n");
		exit(1);
	}

	ProcessGrouper &pgrp = *pgrps.rbegin();
    int rc = 0;
	if( (rc = pgrp.Start(npManager.TakeConfig(), envManager.ToEnvp())) != 0 ) {
        dprintf(ERROR,"execve() %s\n",strerror(errno));
        return Failure;
	}

	fgIndex = pgrps.size() -1;

	return Success;
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





