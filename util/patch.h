#ifndef __PATCH_H__
#define __PATCH_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "minhook/hde32.h"
#include "minhook/include/MinHook.h"

typedef unsigned long DWORD;

typedef int(*fn_search)(char*,size_t,const char*,size_t,size_t);
typedef int(*fn_wildcard_search)(char*,size_t,const char*,size_t,size_t);
typedef MH_STATUS(*fn_MH_CreateHook)(LPVOID,LPVOID,LPVOID*) WINAPI;

extern fn_search _search;
extern fn_wildcard_search _wildcard_search;
extern fn_MH_CreateHook _MH_CreateHook;

char *getDllData(const char *dllFilename, DWORD *dllSize);

bool rva_to_offset(const char *dllFilename, uint32_t rva, uint32_t *offset);
bool offset_to_rva(const char *dllFilename, uint32_t offset, uint32_t *rva);

void patch_memory(uint64_t patch_addr, char *data, size_t len);

int64_t wildcard_find_and_patch_hex(const char *dllFilename, const char *find, uint8_t find_size, int64_t shift, const char *replace, uint8_t replace_size);
int64_t find_and_patch_hex(const char *dllFilename, const char *find, uint8_t find_size, int64_t shift, const char *replace, uint8_t replace_size);

void find_and_patch_string(const char *dllFilename, const char *input_string, const char *new_string);

int search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);
int search_debug(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);
int wildcard_search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);
int wildcard_search_debug(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);

void enable_extended_debug();

#endif