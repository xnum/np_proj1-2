#include "BuiltinHelper.h"


bool BuiltinHelper::IsSupportCmd(string line)
{
	const string cmd[] = {
		"quit",
		"exit",
		"lsjob",
		"setenv",
		"printenv",
		"fg",
        "bg",
        "xnum"
	};

	for( const string& s : cmd ) {
		if( isStartWith(line, s) )
			return true;
	}

	return false;
}

int BuiltinHelper::RunBuiltinCmd(string line)
{
	if( isStartWith(line, "quit") || isStartWith(line, "exit") )
		GoExit();

	if( isStartWith(line, "printenv") || isStartWith(line, "setenv") ) {
		EnvHelper(line);
		return Success;
	}

	if( isStartWith(line, "lsjob") ) {
		procCtrl.printJobs();
		return Success;
	}

	if( isStartWith(line, "fg") ) {
		if( Failure == BringToFront(line) )
			return Failure;
		return Wait;
	}

	if( isStartWith(line, "bg") ) {
		if( Failure == BringToBack(line) )
			return Failure;
		return Success;
	}

    if( isStartWith(line, "xnum") ) {
        godmode = true;
        return Success;
    }

	dprintf(ERROR,"no matching builtin command\n");
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

void BuiltinHelper::EnvHelper(const string& line)
{
	int fg;
	auto cmds = Parser::Parse(line,fg);
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

int BuiltinHelper::BringToFront(const string& line)
{
	int fg = 0;
	auto cmds = Parser::Parse(line,fg);
	int index = -1;
	if( cmds[0].args.size() == 1 ) {
		stringstream ss(cmds[0].args[0]);
		ss >> index;
	}	
	if( Failure == procCtrl.BringToFront(index) ) {
		return Failure;
	}
	return Success;
}

int BuiltinHelper::BringToBack(const string& line)
{
	int fg = 0;
	auto cmds = Parser::Parse(line,fg);
	int index = -1;
	if( cmds[0].args.size() == 1 ) {
		stringstream ss(cmds[0].args[0]);
		ss >> index;
	}	
	if( Failure == procCtrl.BringToBack(index) ) {
		return Failure;
	}
	return Success;
}
