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
		slogf(ERROR,"No processes could be start\n");
		exit(1);
	}

	ProcessGrouper &pgrp = *pgrps.rbegin();
    int rc = pgrp.Start(connfd, npManager.TakeConfig(), envManager.ToEnvp());
	if(rc != Ok) {
        slogf(WARN,"Start failed %s\n",strerror(errno));
	}
    pgrps.pop_back();

	return rc;
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





