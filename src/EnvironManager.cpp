#include "EnvironManager.h"

EnvironManager::EnvironManager()
{
    env_list["PATH"] = "bin:.";
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
