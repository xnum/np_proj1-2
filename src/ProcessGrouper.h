#pragma once

#include "NumberedPipe.h"
#include "Executor.h"

namespace xnsh {
	class ProcessGrouper;
};

using xnsh::ProcessGrouper;

enum StatusResult {
	ProcAllDone,
	ProcNotAllDone,
	ProcNotMine
};

class ProcessGrouper {
	public:
		string originCmds;

		ProcessGrouper(vector<Executor> exes) :
			executors(exes) {}

		int Start(NumberedPipeConfig);
		int NotifyTerminated(pid_t);
		int PassSignal(int sig);
		pid_t GetPgid();

	private:
		vector<Executor> executors;
		pid_t pgid;
};
