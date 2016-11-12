#include "BuiltinHelper.h"

bool BuiltinHelper::IsSupportCmd(string line)
{
	const string cmd[] = {
		"quit",
		"exit",
		"setenv",
		"printenv"
	};

	for( const string& s : cmd ) {
		if( isStartWith(line, s) )
			return true;
	}

	return false;
}

int BuiltinHelper::RunBuiltinCmd(ProcessController& procCtrl, string line)
{
	if( isStartWith(line, "quit") || isStartWith(line, "exit") )
        return Exit;

	if( isStartWith(line, "printenv") || isStartWith(line, "setenv") ) {
		EnvHelper(procCtrl, line);
		return Success;
	}

	slogf(ERROR,"no matching builtin command\n");
	exit(3);
}

bool BuiltinHelper::isStartWith(const string& str, const string& pat)
{
	if( pat.size() > str.size() )
		return false;
	for( size_t i = 0 ; i < pat.size() ; ++i ) {
		if( pat[i] != str[i] )
			return false;
	}
	return true;
}

void BuiltinHelper::GoExit()
{
    close(0);
	exit(0);
}

void BuiltinHelper::EnvHelper(ProcessController& procCtrl, const string& line)
{
	auto cmds = Parser::Parse(line);
	const Command& cmd = cmds[0];

    // % setenv PATH bin
    if( cmd.name == "setenv" && cmd.args.size() == 2 ) {
        procCtrl.envManager.setenv(cmd.args[0], cmd.args[1]);
        return;
    } // % printenv PATH
    else if( cmd.name == "printenv" && cmd.args.size() == 1 ) {
        string val = "";
        if( "" == (val = procCtrl.envManager.getenv(cmd.args[0])) ) {
			printf("getenv error: Not Matched\n");
			return;
        }
        else
            printf("%s=%s\n",cmd.args[0].c_str(), val.c_str());
    }
    else {
        puts("Command Example: ");
        puts("% setenv PATH bin");
        puts("% printenv PATH");
    }
}


