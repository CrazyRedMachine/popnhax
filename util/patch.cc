// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#include "util/search.h"
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


bool rva_to_offset(const char *dllFilename, uint32_t rva, uint32_t *offset)
{
  uintptr_t baseAddr = (uintptr_t)GetModuleHandle(dllFilename);
  IMAGE_DOS_HEADER * pDosHdr = (IMAGE_DOS_HEADER *) baseAddr;
  IMAGE_NT_HEADERS * pNtHdr = (IMAGE_NT_HEADERS *) (baseAddr + pDosHdr->e_lfanew);
	
  int i;
  WORD wSections;
  PIMAGE_SECTION_HEADER pSectionHdr;
  pSectionHdr = IMAGE_FIRST_SECTION(pNtHdr);
  wSections = pNtHdr -> FileHeader.NumberOfSections;
  for (i = 0; i < wSections; i++)
  {
    if (pSectionHdr -> VirtualAddress <= rva)
      if ((pSectionHdr -> VirtualAddress + pSectionHdr -> Misc.VirtualSize) > rva)
    {
      rva -= pSectionHdr -> VirtualAddress;
      rva += pSectionHdr -> PointerToRawData;
      *offset = rva;
	  return true;
    }
    pSectionHdr++;
  }
  return false;
}

bool offset_to_rva(const char *dllFilename, uint32_t offset, uint32_t *rva)
{
  uintptr_t baseAddr = (uintptr_t)GetModuleHandle(dllFilename);
  IMAGE_DOS_HEADER * pDosHdr = (IMAGE_DOS_HEADER *) baseAddr;
  IMAGE_NT_HEADERS * pNtHdr = (IMAGE_NT_HEADERS *) (baseAddr + pDosHdr->e_lfanew);
	
  int i;
  WORD wSections;
  PIMAGE_SECTION_HEADER pSectionHdr;

  pSectionHdr = IMAGE_FIRST_SECTION(pNtHdr);
  wSections = pNtHdr -> FileHeader.NumberOfSections;

  for (i = 0; i < wSections; i++) {
    if (pSectionHdr -> PointerToRawData <= offset)
      if ((pSectionHdr -> PointerToRawData + pSectionHdr -> SizeOfRawData) > offset) {
        offset -= pSectionHdr -> PointerToRawData;
        offset += pSectionHdr -> VirtualAddress;

		*rva = offset;
        return true;
      }

    pSectionHdr++;
  }

  return false;
}

void find_and_patch_string(const char *dllFilename, const char *input_string, const char *new_string) {
    DWORD dllSize = 0;
    char *data = getDllData(dllFilename, &dllSize);

    while (1) {
        int64_t pattern_offset = search(data, dllSize, input_string, strlen(input_string), 0);
        if (pattern_offset == -1) {
            break;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        char *new_string_buff = (char*)calloc(strlen(new_string) + 1, sizeof(char));
        memcpy(new_string_buff, new_string, strlen(new_string));
        patch_memory(patch_addr, new_string_buff, strlen(new_string) + 1);
        free(new_string_buff);
    }
}

bool find_and_patch_hex(const char *dllFilename, const char *find, uint8_t find_size, int64_t shift, const char *replace, uint8_t replace_size) {
    DWORD dllSize = 0;
    char *data = getDllData(dllFilename, &dllSize);

    int64_t pattern_offset = search(data, dllSize, find, find_size, 0);
    if (pattern_offset == -1) {
        return false;
    }

#if DEBUG == 1
    LOG("BEFORE PATCH :\n");
    uint8_t *offset = (uint8_t *) ((int64_t)data + pattern_offset + shift - 5);
    for (int i=0; i<32; i++)
    {
        LOG("%02x ", *offset);
        offset++;
        if (i == 15)
            LOG("\n");
    }
#endif

    uint64_t patch_addr = (int64_t)data + pattern_offset + shift;
    patch_memory(patch_addr, (char *)replace, replace_size);

#if DEBUG == 1
    LOG("\nAFTER PATCH :\n");
    offset = (uint8_t *) ((int64_t)data + pattern_offset + shift - 5);
    for (int i=0; i<32; i++)
    {
        LOG("%02x ", *offset);
        offset++;
        if (i == 15)
            LOG("\n");
    }
#endif

    return true;

}