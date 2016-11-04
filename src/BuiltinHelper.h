#pragma once
#include "ProcessController.h"
#include "EnvironManager.h"
#include "Logger.h"

#define BH_IF_IS(rc,param) ((rc&param))

namespace xnsh {
	class BuiltinHelper;
};

using xnsh::BuiltinHelper;


class BuiltinHelper {
public:
	static bool IsSupportCmd(string);
	static int RunBuiltinCmd(ProcessController&, string);
private:
	static bool isStartWith(const string& str, const string& pat);

	static void GoExit();
	static void EnvHelper(ProcessController&, const string&);
	static int BringToFront(ProcessController&, const string&);
    static int BringToBack(ProcessController&, const string&);
};


