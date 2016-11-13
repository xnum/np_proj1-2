#include "Executor.h"

Executor::Executor(const Command& cmd)
    : cmdHnd(cmd)
{
    done = false;
}
