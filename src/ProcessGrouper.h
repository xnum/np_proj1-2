#pragma once
#include <sys/time.h>
#include <sys/resource.h>

#include "Logger.h"
#include "NumberedPipe.h"
#include "Executor.h"
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

		int Start(NumberedPipeConfig,char**);

	private:
		vector<Executor> executors;
};
