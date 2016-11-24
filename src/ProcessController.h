#pragma once

enum SimpleRetVal {
    Failure,
    Success,
    Exit
};

#include "Logger.h"
#include "Executor.h"
#include "ProcessGrouper.h"
#include "EnvironManager.h"

class ProcessController {
public:
    ProcessController();
    int AddProcGroups(const vector<Executor>&, const string& cmd);
    int StartProc();
    string ToPathname(string filename);
    void SetupPwd();

    EnvironManager envManager;
    NumberedPipeManager npManager;
    string pwd;
    int connfd;

private:
    vector<ProcessGrouper> pgrps;
};
