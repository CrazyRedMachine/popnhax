#ifndef __SEARCHFILE_H_
#define __SEARCHFILE_H_
#include <vector>
#include <string>
using namespace std;
class SearchFile
{
private:
    vector<string>  result;
public:
    vector<string>  getResult();
    bool search(const char *path,const char *ext,bool subfolders_only);
};
#endif
