#pragma once

enum SimpleRetVal {
	Failure,
	Success,
    Wait,
    Exit
};

enum PgidTarget {
	Shell = -1,
	ForeGround = 0
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
	private:
		vector<ProcessGrouper> pgrps;
		int fgIndex;
};
