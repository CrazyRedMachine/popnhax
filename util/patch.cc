// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#include "patch.h"

void patch_memory(uint64_t patch_addr, char *data, size_t len) {
    DWORD old_prot;
    VirtualProtect((LPVOID)patch_addr, len, PAGE_EXECUTE_READWRITE, &old_prot);
    memcpy((LPVOID)patch_addr, data, len);
    VirtualProtect((LPVOID)patch_addr, len, old_prot, &old_prot);
}

char *getDllData(const char *dllFilename, DWORD *dllSize) {
    HMODULE _moduleBase = GetModuleHandle(dllFilename);
    MODULEINFO module_info;

    memset(&module_info, 0, sizeof(module_info));
    if (!GetModuleInformation(GetCurrentProcess(), _moduleBase, &module_info,
                              sizeof(module_info))) {
        return NULL;
    }

    *dllSize = module_info.SizeOfImage;
    return (char *)module_info.lpBaseOfDll;
}
