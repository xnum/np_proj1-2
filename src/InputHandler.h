#pragma once

#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <termios.h> // for tcxxxattr, ECHO, etc ..
#include <unistd.h> // for STDIN_FILENO

using namespace std;

class InputHandler {
public:
    string Getline();

private:
    vector<string> history;
};
