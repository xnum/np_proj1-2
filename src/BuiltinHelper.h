#pragma once
#include "ProcessController.h"
#include "EnvironManager.h"
#include "Message.h"
#include "Logger.h"

extern ProcessController procCtrl;
extern MessageCenter msgCenter;

#define BH_IF_IS(rc,param) ((rc&param))

class BuiltinHelper {
public:
	static bool IsSupportCmd(string);
	static int RunBuiltinCmd(string);
private:
	static bool isStartWith(const string& str, const string& pat);

	static void GoExit();
	static void EnvHelper(const string&);
	static int BringToFront(const string&);
    static int BringToBack(const string&);
};


