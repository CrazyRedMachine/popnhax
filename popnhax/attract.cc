#include <cstdio>
#include <cstring>
#include <io.h>
#include <windows.h>

#include "SearchFile.h"

#include "util/search.h"

#include "util/log.h"
#include "util/patch.h"

#include "tableinfo.h"
#include "loader.h"

#include "imports/avs.h"
#include "xmlhelper.h"

#include "minhook/hde32.h"
#include "minhook/include/MinHook.h"

#include "custom_categs.h"

extern const char* g_game_dll_fn;

uint32_t g_autoplay_marker_addr = 0;
uint32_t g_attract_marker_addr = 0;
uint32_t g_is_button_pressed_fn = 0;
uint32_t g_interactive_cooldown = 0;

void (*real_attract)(void);
void hook_ex_attract()
{
    __asm("push ebx\n");
    __asm("mov ebx, eax\n");
    __asm("add ebx, 2\n");
    __asm("mov byte ptr [ebx], 3\n"); //force EX chart_num
    __asm("add ebx, 8\n");
    __asm("mov byte ptr [ebx], 40\n"); // force x4.0 multiplier
    __asm("pop ebx\n");
    real_attract();
}

bool patch_ex_attract()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\x81\xE7\x01\x00\x00\x80\x79\x05\x4F", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_ex: cannot find attract mode song info\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_ex_attract,
                     (void **)&real_attract);
    }
    LOG("popnhax: attract mode will play EX charts at 4.0x hispeed\n");
    return true;
}

bool patch_full_attract()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\xB8\xD0\x07\x00\x00\x66\xA3", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_full: cannot find attract mode timer set function\n");
            return false;
        }

        uint32_t timer_addr = *(uint32_t*)((int64_t)data + pattern_offset + 7);
        uint8_t new_pattern[9] = "\x66\x83\x05\x00\x00\x00\x00\xFF";
        memcpy(new_pattern+3, &timer_addr, 4);

        if (!find_and_patch_hex(g_game_dll_fn, (char*)new_pattern, 8, 7, "\x00", 1))
        {
            LOG("popnhax: attract_full: cannot stop attract mode song timer\n");
            return false;
        }
    }
    LOG("popnhax: attract mode will play full songs\n");
    return true;
}

void (*real_attract_inter)(void);
void hook_attract_inter()
{
 __asm("push 0x1EF\n");
 __asm("mov eax, dword ptr [_g_is_button_pressed_fn]\n");
 __asm("call eax\n");
 __asm("add esp, 4\n");
 __asm("test al, al\n");
 __asm("je skip_disable_autoplay\n");
 __asm("mov eax, dword ptr [_g_autoplay_marker_addr]\n");
 __asm("mov dword ptr [eax], 0\n");
 __asm("skip_disable_autoplay:");
 real_attract_inter();
}

void (*real_retire_handling)(void);
void hook_attract_inter_rearm()
{
 __asm("push eax\n");
 __asm("mov eax, dword ptr [_g_attract_marker_addr]\n");
 __asm("cmp word ptr [eax], 0x0101\n");
 __asm("jne skip_rearm_autoplay\n");
 __asm("mov eax, dword ptr [_g_autoplay_marker_addr]\n");
 __asm("mov dword ptr [eax], 1\n");
 __asm("skip_rearm_autoplay:");
 __asm("pop eax\n");
 real_retire_handling();
}

void (*real_songend_handling)(void);
void hook_attract_inter_songend_rearm()
{
 __asm("push eax\n");
 __asm("mov eax, dword ptr [_g_attract_marker_addr]\n");
 __asm("cmp word ptr [eax], 0x0101\n");
 __asm("jne skip_se_rearm_autoplay\n");
 __asm("mov eax, dword ptr [_g_autoplay_marker_addr]\n");
 __asm("mov dword ptr [eax], 1\n");
 __asm("skip_se_rearm_autoplay:");
 __asm("pop eax\n");
 real_songend_handling();
}

void (*real_test_handling)(void);
void hook_attract_inter_rearm_test()
{
 __asm("push eax\n");
 __asm("mov eax, dword ptr [_g_attract_marker_addr]\n");
 __asm("cmp word ptr [eax], 0x0101\n");
 __asm("jne skip_rearm_autoplay_test\n");
 __asm("mov eax, dword ptr [_g_autoplay_marker_addr]\n");
 __asm("mov dword ptr [eax], 1\n");
 __asm("skip_rearm_autoplay_test:");
 __asm("pop eax\n");
 real_test_handling();
}

void (*real_rearm_marker)(void);
void hook_retrieve_attractmarker()
{
 __asm("add esi, 0x18\n"); 
 __asm("mov dword ptr [_g_attract_marker_addr], esi\n");
 __asm("sub esi, 0x18\n");
 real_rearm_marker();
}
bool patch_attract_interactive()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = search(data, dllSize, "\x33\xC4\x89\x44\x24\x0C\x56\x57\x53\xE8", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_interactive: cannot find set autoplay marker function call\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x14;

        uint32_t function_offset = *((uint32_t*)(patch_addr+0x01));
        uint64_t function_addr = patch_addr+5+function_offset;
		
		g_autoplay_marker_addr = *((uint32_t*)(function_addr+0x02));
	}
    {
        int64_t pattern_offset = search(data, dllSize, "\x00\x00\x88\x46\x18\x88\x46", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_interactive: cannot find get songinfozone function call\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x02;

        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_retrieve_attractmarker,
                     (void **)&real_rearm_marker);
		
	}
		//bad version
		//g_attract_marker_addr = g_autoplay_marker_addr - 0x105; //TODO: check other versions peace ya 8 de moins
		//exit(0);
		 

    /* enable interactive mode on button press (except red) */
    {
        int64_t pattern_offset = wildcard_search(data, dllSize, "\xCC\xCC\x53\x32\xDB\xE8????\x84\xC0\x74\x78", 14, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_interactive: cannot find attract mode demo loop function\n");
            return false;
        }

        int64_t pattern_offset2 = search(data, dllSize-pattern_offset, "\x6A\x10\xE8", 3, pattern_offset);
        if (pattern_offset2 == -1) {
            LOG("popnhax: attract_interactive: cannot find isButtonPressed function\n");
            return false;
        }
        uint64_t patch_addr2 = (int64_t)data + pattern_offset2 + 0x02;
        uint32_t function_offset = *((uint32_t*)(patch_addr2+0x01));
        g_is_button_pressed_fn = patch_addr2+5+function_offset;

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x02;
        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_attract_inter,
                     (void **)&real_attract_inter);

    }

    /* disable interactive mode after a while without button press */
	{
        int64_t pattern_offset = search(data, dllSize, "\x3D\x58\x02\x00\x00\x7C", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_interactive: cannot find retire handling\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x07;

        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_attract_inter_rearm,
                     (void **)&real_retire_handling);
    }

    /* fix end of song crash */
    {
        int64_t pattern_offset = search(data, dllSize, "\xB8\xD0\x07\x00\x00\x66\xA3", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_full: cannot find attract mode timer set function\n");
            return false;
        }

        uint32_t timer_addr = *(uint32_t*)((int64_t)data + pattern_offset + 7);
        uint8_t new_pattern[8] = "\x66\x83\x05\x00\x00\x00\x00";
        memcpy(new_pattern+3, &timer_addr, 4);

        pattern_offset = search(data, dllSize, (const char *)new_pattern, 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_interactive: cannot find attract mode timer set function\n");
            return false;
        }
		int64_t pattern_offset2 = search(data, dllSize-pattern_offset, "\x66\x85\xC0\x74", 4, pattern_offset);
        if (pattern_offset2 == -1) {
            LOG("popnhax: attract_interactive: cannot find end of song handling function\n"); //TODO: FIX FOR PEACE and maybe other
            return false;
        }

		uint64_t patch_addr = (int64_t)data + pattern_offset2 + 0x05;

		MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_attract_inter_songend_rearm,
                     (void **)&real_songend_handling);
		/* mauvais endroit, commun à attract et regular (par contre dans cette zone faudra reset le marker autoplay quand on arrive en fin de song pour pas crash)
        a partir de pattern offset, on cherche 74 0B 89 2D + 0x02 , idem on check si 01 01 auquel cas on remet autoplay à 1
		                                          19    3d
		uint64_t patch_addr = (int64_t)data + pattern_offset - 0x1B;
        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_attract_inter,
                     (void **)&real_attract_inter);
*/
    }
	
	/* fix crash when pressing test button during interactive mode */
	{
        int64_t pattern_offset = search(data, dllSize, "\x83\xC4\x04\x84\xC0\x74\x75\x38\x1D", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: attract_interactive: cannot find test button handling\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x07;

        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_attract_inter_rearm_test,
                     (void **)&real_test_handling);
    }
	
    LOG("popnhax: attract mode is interactive\n");
    return true;

}