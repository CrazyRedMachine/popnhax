#ifndef __TRANSLATION_H__
#define __TRANSLATION_H__

#include <stdint.h>

FILE* _translation_open_dict(char *foldername, bool *ips);

bool patch_translate(const char *dllFilename, const char *folder, bool debug);

#endif
