#include "EnvironManager.h"

EnvironManager::EnvironManager()
{
    env_list["PATH"] = "bin:.";
    envp = NULL;
}

int EnvironManager::setenv(const string& key, const string& val)
{
    env_list[key] = val;
    return 0;
}

string EnvironManager::getenv(const string& key)
{
    const auto& it = env_list.find(key);
    if( it == env_list.end() )
        return "";
    else
        return it->second;
}

char** EnvironManager::ToEnvp()
{
    Free();
    envp = (char**)calloc(env_list.size()+1, sizeof(char*));
    int count = 0;
    for(auto it : env_list) {
        string tmp = ((it.first)+"="+(it.second));
        envp[count++] = strdup(tmp.c_str());
    }
    envp[env_list.size()] = NULL;

    return envp;
}

void EnvironManager::Free()
{
    if(envp!=NULL)
    {
        char **ptr = envp;
        while( *ptr != NULL )
            free(*ptr++);
        free(envp);
    }
}
