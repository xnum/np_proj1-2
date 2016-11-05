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

namespace xnsh {
	class ProcessController;
};

#include "Logger.h"
#include "Executor.h"
#include "ProcessGrouper.h"
#include "EnvironManager.h"

using xnsh::ProcessController;

class ProcessController {
	public:
        ProcessController();
		int AddProcGroups(const vector<Executor>&, const string& cmd);
		int StartProc(bool isfg);
		int TakeTerminalControl(pid_t);
		void SetShellPgid(pid_t p) { shellPgid = p; }
		int FreeProcess(pid_t);
		int SendSignalToFG(int sig);
		void BackToShell();
		int BringToFront(int index);
		int BringToBack(int index);
		void printJobs();
		void RefreshJobStatus();
        string ToPathname(string filename);
        void SetupPwd();

        EnvironManager envManager;
        NumberedPipeManager npManager;
        string pwd;
	private:
		vector<ProcessGrouper> pgrps;
		int fgIndex;
		pid_t shellPgid;
};
