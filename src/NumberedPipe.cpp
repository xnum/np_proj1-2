#include "NumberedPipe.h"

int NumberedPipeManager::CutNumberedPipeToken(string& line)
{
    NumberedPipe np;
    string newline;

    stringstream ss(line);
    string token;
    while(ss >> token) {
        bool takeIt = false;
        if(token.size() > 1) {
            if(token[0] == '|') {
                int n = -1;
                if(1 == sscanf(token.c_str(), "|%d", &n))
                    np.redirOut = n;
                takeIt = true;
            }

            if(token[0] == '!') {
                int n = -1;
                if(1 == sscanf(token.c_str(), "!%d", &n))
                    np.redirErr = n;
                takeIt = true;
            }
        }

        if(!takeIt)
            newline += " " + token + " ";
    }

    np.Print();
    
    line = newline;

    return 0;
}
