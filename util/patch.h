#ifndef __PATCH_H__
#define __PATCH_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned long DWORD;

void patch_memory(uint64_t patch_addr, char *data, size_t len);
char *getDllData(const char *dllFilename, DWORD *dllSize);

#endif