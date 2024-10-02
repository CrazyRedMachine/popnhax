#ifndef __PATCH_H__
#define __PATCH_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef unsigned long DWORD;

char *getDllData(const char *dllFilename, DWORD *dllSize);

bool rva_to_offset(const char *dllFilename, uint32_t rva, uint32_t *offset);
bool offset_to_rva(const char *dllFilename, uint32_t offset, uint32_t *rva);

void patch_memory(uint64_t patch_addr, char *data, size_t len);

int64_t find_and_patch_hex(const char *dllFilename, const char *find, uint8_t find_size, int64_t shift, const char *replace, uint8_t replace_size);
void find_and_patch_string(const char *dllFilename, const char *input_string, const char *new_string);

#endif