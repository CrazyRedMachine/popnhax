
#include <windows.h>
#include "SearchFile.h"

vector<string>  SearchFile::getResult()
{
    auto   t = result;
    result.clear();
    return t;
}

bool SearchFile::search(const char *path, const char *ext, bool subfolders_only)
{
    HANDLE hFile;
    char   buffer[MAX_PATH]={0,};
    WIN32_FIND_DATA pNextInfo;
    string  t;

    sprintf(buffer,"%s\\*.*",path);
    hFile = FindFirstFileA(buffer,&pNextInfo);
    if(!hFile){
        return false;
    }

    while(FindNextFileA(hFile,&pNextInfo))
    {
        if(pNextInfo.cFileName[0] == '.')// . and ..
            continue;

        if(pNextInfo.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY && subfolders_only)
        {
            ZeroMemory(buffer,MAX_PATH);
            sprintf(buffer,"%s\\%s",path,pNextInfo.cFileName);
            search(buffer,ext,false);
        }
        if (!subfolders_only)
        {
            t.assign(path);
            t+='\\';
            t.append(pNextInfo.cFileName);
            if(t.substr(t.size()-strlen(ext))==ext)
            {
                result.push_back(t);
            }
        }
    }
    return true;
}

