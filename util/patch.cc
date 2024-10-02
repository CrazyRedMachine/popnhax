// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#include "util/log.h"
#include "libdisasm/libdis.h"

#include "patch.h"

#define LINE_SIZE 512
#define NO_OF_CHARS 256

// A utility function to get maximum of two integers
static int max(int a, int b) {
    return (a > b) ? a : b;
}

// The preprocessing function for Boyer Moore's bad character heuristic
static void badCharHeuristic(const unsigned char *str, int size, int* badchar, bool wildcards) {
    int i;

    // Initialize all occurrences as -1
    for (i = 0; i < NO_OF_CHARS; i++)
        badchar[i] = -1;

    // Fill the actual value of last occurrence of a character
    if (wildcards)
    {
        int lastwildcard = -1;
        for (i = 0; i < size; i++)
        {
            if (str[i] != '?')
                badchar[(int) str[i]] = i;
            else
                lastwildcard = i;
        }

        for (i = 0; i < NO_OF_CHARS; i++)
        {
            if ( badchar[i] < lastwildcard )
                badchar[i] = lastwildcard;
        }

    } else {
        for (i = 0; i < size; i++)
            badchar[(int) str[i]] = i;
    }
}

#define DEBUG_SEARCH 0

int _search_ex(unsigned char *haystack, size_t haystack_size, const unsigned char *needle, size_t needle_size, int orig_offset, bool wildcards, int debug) {
    int badchar[NO_OF_CHARS];

    badCharHeuristic(needle, needle_size, badchar, wildcards);

    int64_t s = 0; // s is shift of the pattern with respect to text
    while (s <= (haystack_size - needle_size)) {
        int j = needle_size - 1;
 if (debug == 2)
 {
        LOG("--------------------------------\n");
        LOG("txt...");
        for (size_t i = 0; i < needle_size; i++)
        {
            LOG("%02x ", haystack[orig_offset+s+i]);
        }
        LOG("\n");
        LOG("pat...");
        for (size_t i = 0; i < needle_size; i++)
        {
            if (wildcards && needle[i] == '?')
                LOG("** ");
            else
                LOG("%02x ", needle[i]);
        }
        LOG("\n");
 }
        if ( wildcards )
        {
            while (j >= 0 && ( needle[j] == '?' || needle[j] == haystack[orig_offset + s + j]) )
                j--;
        } else {
            while (j >= 0 && ( needle[j] == haystack[orig_offset + s + j]) )
                j--;
        }

        if (j < 0) {
                if (debug)
                LOG("found string at offset %llx!\n", orig_offset +s);
            return orig_offset + s;
        }
        else
        {
            s += max(1, j - badchar[(int)haystack[orig_offset + s + j]]);
            if (debug)
                LOG("mismatch at pos %d, new offset %llx\n\n", j, orig_offset+s);
        }
    }

    return -1;
}

int search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset) {
    int res = _search_ex((unsigned char*) haystack, haystack_size, (const unsigned char *)needle, needle_size, orig_offset, false, 0);
    return res;
}

int wildcard_search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset) {
    int res = _search_ex((unsigned char*) haystack, haystack_size, (const unsigned char *)needle, needle_size, orig_offset, true, 0);
    return res;
}

int search_debug(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset) {
    int res = _search_ex((unsigned char*) haystack, haystack_size, (const unsigned char *)needle, needle_size, orig_offset, false, 2);
    return res;
}

int wildcard_search_debug(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset) {
    int res = _search_ex((unsigned char*) haystack, haystack_size, (const unsigned char *)needle, needle_size, orig_offset, true, 2);
    return res;
}

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
        int64_t pattern_offset = _search(data, dllSize, input_string, strlen(input_string), 0);
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

int64_t find_and_patch_hex(const char *dllFilename, const char *find, uint8_t find_size, int64_t shift, const char *replace, uint8_t replace_size) {
    DWORD dllSize = 0;
    char *data = getDllData(dllFilename, &dllSize);

    int64_t pattern_offset = _search(data, dllSize, find, find_size, 0);
    if (pattern_offset == -1) {
        return 0;
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

    return pattern_offset;

}

void log_cb(x86_insn_t *insn, void *arg)
{
    char line[LINE_SIZE];    /* buffer of line to print */
    x86_format_insn(insn, line, LINE_SIZE, intel_syntax);
    LOG("%s\n", line);
}

MH_STATUS WINAPI patch_debug_MH_CreateHook(LPVOID patch_addr, LPVOID hook_function, LPVOID* real_function){
    LOG("--- hooking function over this code ---\n");
    x86_init(opt_none, NULL, NULL);
    x86_disasm_range( (unsigned char *)patch_addr, 0, 0, 50, log_cb, NULL );
/*
    int size = x86_disasm((unsigned char*)buf, dllSize, 0, ((uint32_t)(hook_addrs[i])-(uint32_t)buf+delta), &insn);
    if ( size ) {
        x86_format_insn(&insn, line, LINE_SIZE, intel_syntax);
        membuf_printf(membuf, "\t\t<!-- %s -->\n", line);
        x86_oplist_free(&insn);
    }
*/
    x86_cleanup();
    LOG("------\n");

    return MH_CreateHook(patch_addr, hook_function, real_function);
}

int patch_debug_search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset)
{
    LOG("--- Looking for pattern ");
    for (size_t i = 0; i<needle_size; i++)
    {
        LOG("\\x%.02X", (unsigned char)needle[i]);
    }
    LOG("---\n");
    int found = search(haystack, haystack_size, needle, needle_size, orig_offset);
    if ( found != -1 )
    {
        LOG("--- found pattern at this code ---\n");
        x86_init(opt_none, NULL, NULL);
        x86_disasm_range( (unsigned char*)(haystack+found), 0, 0, 50, log_cb, NULL );
        x86_cleanup();
        LOG("------\n");
    }
    else
    {
        LOG("--- pattern not found ---\n");
    }
    return found;
}

int patch_debug_wildcard_search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset)
{
    LOG("--- Looking for wildcard pattern ");
    for (size_t i = 0; i<needle_size; i++)
    {
        LOG("\\x%.02X", (unsigned char)needle[i]);
    }
    LOG("---\n");
    int found = wildcard_search(haystack, haystack_size, needle, needle_size, orig_offset);
    if ( found != -1 )
    {
        LOG("--- found wildcard pattern at this code ---\n");
        x86_init(opt_none, NULL, NULL);
        x86_disasm_range((unsigned char*)(haystack+found), 0, 0, 50, log_cb, NULL );
        x86_cleanup();
        LOG("\n");
    }
    else
    {
        LOG("--- wildcard pattern not found ---\n");
    }
    return found;
}

fn_search _search = search;
fn_wildcard_search _wildcard_search = wildcard_search;
fn_MH_CreateHook _MH_CreateHook = MH_CreateHook;

void enable_extended_debug()
{
    _search = patch_debug_search;
    _wildcard_search = patch_debug_wildcard_search;
    _MH_CreateHook = patch_debug_MH_CreateHook;
}
