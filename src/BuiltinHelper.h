#pragma once
#include "ProcessController.h"
#include "EnvironManager.h"
#include "Logger.h"
#include "Message.h"

#define BH_IF_IS(rc, param) ((rc& param))

extern MessageCenter msgCenter;

class BuiltinHelper {
public:
    static bool IsSupportCmd(string);
    static int RunBuiltinCmd(ProcessController&, string);

private:
    static bool isStartWith(const string& str, const string& pat);

    static void GoExit();
    static void EnvHelper(ProcessController&, const string&);
};
