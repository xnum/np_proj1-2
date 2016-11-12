#pragma once
#include <sys/time.h>
#include <sys/resource.h>

#include "Logger.h"
#include "NumberedPipe.h"
#include "Executor.h"

enum StartResult {
    Ok = 0,
    Wait = 1,
    Fail = 2
};

class ProcessGrouper {
	public:
		string originCmds;

		ProcessGrouper(vector<Executor> exes) :
			executors(exes) {}

		int Start(int connfd,NumberedPipeConfig,char**);

	private:
		vector<Executor> executors;
};
