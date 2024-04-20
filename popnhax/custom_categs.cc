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

bool g_subcategmode = false;
uint32_t g_min_id = 4000;
uint32_t g_max_id = 0;

const char *g_categname = "Custom Tracks";
const char *g_categicon = "cate_13";
const char *g_categformat = "[ol:4][olc:d92f0d]%s";

char *g_string_addr;
uint8_t idx = 0;
uint32_t tmp_size = 0;
uint32_t tmp_categ_ptr = 0;
uint32_t tmp_songlist_ptr = 0;

#define SIMPLE_CATEG_ALLOC 1

#if SIMPLE_CATEG_ALLOC == 1
uint32_t *songlist;
#else
uint32_t songlist[4096] = {0};
#endif
uint32_t songlist_addr = (uint32_t)&songlist;
uint32_t songlist_count = 0;

struct songlist_struct_s {
    uint32_t dummy[3];
    uint32_t array_start;
    uint32_t array_end;
} songlist_struct;
uint32_t songlist_struct_addr = (uint32_t)&songlist_struct;

typedef struct {
    char *name;
    uint32_t size;
    uint32_t *songlist;
} subcategory_s;

subcategory_s* subcategories;
uint32_t g_subcateg_count = 0;
//subcategories[0] is the "ALL SONGS" virtual subcategory,
//actual subcategories then go from subcategories[1] to subcategories[g_subcateg_count]

struct property *load_prop_file(const char *filename);

static bool subcateg_has_songid(uint32_t songid, subcategory_s* subcateg)
{
    for ( uint32_t i = 0; i < subcateg->size; i++ )
    {
        if ( subcateg->songlist[i] == songid )
            return true;
    }
    return false;
}

static void add_song_to_subcateg(uint32_t songid, subcategory_s* subcateg)
{
    if ( songid >= g_min_id
     && (g_max_id == 0 || songid <= g_max_id)
     && !subcateg_has_songid(songid, subcateg) )
    {
        subcateg->songlist = (uint32_t *) realloc(subcateg->songlist, sizeof(uint32_t)*(++subcateg->size));
        subcateg->songlist[subcateg->size-1] = songid;
        subcateg->songlist[subcateg->size-1] |= 0x00060000; // game wants this otherwise only easy difficulty will appear

        // Also add it to the "ALL SONGS" subcategory
        subcategories[0].songlist = (uint32_t *) realloc(subcategories[0].songlist, sizeof(uint32_t)*(++subcategories[0].size));
        subcategories[0].songlist[subcategories[0].size-1] = subcateg->songlist[subcateg->size-1];
    }
}

static subcategory_s* get_subcateg(char *title)
{
    for (uint32_t i = 0; i < g_subcateg_count; i++)
    {
        //iterating over actual subcategories, excluding "ALL SONGS"
        if (strcmp(title, subcategories[i+1].name) == 0)
            return &(subcategories[i+1]);
    }
    return NULL;
}

void (*add_song_in_list)();
void (*categ_inject_songlist)();

struct songlist_struct_s *new_song_list = NULL;
void get_subcateg_size_impl()
{
    __asm("push edx\n");
    __asm("mov _idx, eax\n");

    tmp_size = subcategories[idx].size;
    new_song_list = (struct songlist_struct_s*) songlist_struct_addr;
    new_song_list->array_start = (uint32_t)&(subcategories[idx].songlist[0]);
    new_song_list->array_end = (uint32_t)&(subcategories[idx].songlist[tmp_size]);

    __asm("mov eax, [_tmp_size]");
    __asm("mov ecx, _new_song_list");
    __asm("pop edx\n");
}

uint32_t tmp_str_addr;
void (*real_event_categ_generation)();
void hook_event_categ_generation()
{
    //[esp+0x1C] contains the subcategory name string pointer, I compare it with my subcategories string pointers..
    // no match found -> we're actually processing the EVENT category, leave
    // match found -> I need to fix eax with a pointer to my songlist before the subcall to the subcategory generating function
    __asm("mov _new_song_list, eax"); //save original intended value
    __asm("push ecx");
    __asm("push edx");
    __asm("mov ebx, [esp+0x30]\n"); // +0x30 due to a couple implicit "push" added by the compiler
    __asm("mov _tmp_str_addr, ebx\n");

    for (uint32_t i = 0; i < g_subcateg_count+1; i++)
    {
        if (  (uint32_t)subcategories[i].name == tmp_str_addr )
        {
            tmp_size = subcategories[i].size;
            new_song_list = (struct songlist_struct_s*) songlist_struct_addr;
            new_song_list->array_start = (uint32_t)&(subcategories[i].songlist[0]);
            new_song_list->array_end = (uint32_t)&(subcategories[i].songlist[tmp_size]);
            break;
        }
    }
    __asm("pop edx");
    __asm("pop ecx");
    __asm("mov eax, _new_song_list");
    real_event_categ_generation();
}

void get_subcateg_name_impl()
{
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("mov _idx, eax\n");
    g_string_addr = subcategories[idx].name;
    __asm("mov eax, _g_string_addr");
    __asm("pop edx\n");
    __asm("pop ecx\n");
}

uint32_t reimpl_value_1;
uint32_t reimpl_value_2;
void (*get_subcateg_size)() = &get_subcateg_size_impl;
void (*get_subcateg_name)() = &get_subcateg_name_impl;
void (*reimpl_func_1)();
void (*reimpl_func_2_generate_event_category)();
void (*reimpl_func_3)();
void (*reimpl_func_4)();

//this is a reimplementation of the event category generation function, modded to use popnhax internal subcategories
void categ_inject_songlist_subcategs()
{
    __asm("add esp, 0xC"); // cancel a sub esp 0xC that is added by this code for no reason
    __asm("push 0xFFFFFFFF\n");
    __asm("push [_reimpl_value_1]\n");
    __asm("mov eax, dword ptr fs:[0]\n");
    __asm("push eax\n");
    __asm("sub esp, 0x10\n");
    __asm("push ebx\n");
    __asm("push ebp\n");
    __asm("push esi\n");
    __asm("push edi\n");

    __asm("push ebx\n");
    __asm("mov ebx, dword ptr ds:[_reimpl_value_2]\n");
    __asm("mov eax, [ebx]\n");
    __asm("pop ebx\n");

    __asm("xor eax,esp\n");
    __asm("push eax\n");
    __asm("lea eax, dword ptr [esp+0x24]\n");
    __asm("mov dword ptr fs:[0], eax\n");
    __asm("mov ebp, dword ptr [esp+0x34]\n");
    __asm("xor ebx, ebx\n");

    __asm("mov dword ptr ss:[esp+0x34], ebx\n");
    __asm("subcateg_loop:\n");
    __asm("mov ecx, dword ptr ss:[ebp+0x08]\n");
    __asm("mov eax, ebx\n");

    get_subcateg_size();

    __asm("test eax, eax\n");
    __asm("je next_iter\n");
    __asm("push 0xD8\n");

    reimpl_func_1();

    __asm("mov ecx, eax\n");
    __asm("add esp, 0x04\n");
    __asm("mov dword ptr ss:[esp+0x14], ecx\n");
    __asm("xor eax, eax\n");
    __asm("mov dword ptr ss:[esp+0x2C], eax\n");
    __asm("cmp ecx, eax\n");
    __asm("je jump_point_1\n");
    __asm("mov eax, ebx\n");

    get_subcateg_name();

    __asm("mov edi, dword ptr ss:[ebp+0x04]\n");
    __asm("mov edx, dword ptr ss:[ebp+0x0C]\n");
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("mov ecx, dword ptr ss:[ebp+0x44]\n");

    reimpl_func_2_generate_event_category();

    __asm("jump_point_1:\n");
    __asm("mov dword ptr ss:[esp+0x2C], 0xFFFFFFFF\n");
    __asm("mov ecx, dword ptr ss:[ebp+0xB4]\n");
    __asm("lea esi, dword ptr [ebp+0xA8]\n");
    __asm("mov dword ptr ss:[esp+0x14], eax\n");
    __asm("test ecx, ecx\n");
    __asm("jne jump_point_2\n");
    __asm("xor edx, edx\n");
    __asm("jmp jump_point_3\n");
    __asm("jump_point_2:\n");
    __asm("mov edx, dword ptr ds:[esi+0x14]\n");
    __asm("sub edx, ecx\n");
    __asm("sar edx, 0x02\n");
    __asm("jump_point_3:\n");
    __asm("mov edi, dword ptr ds:[esi+0x10]\n");
    __asm("mov ebx, edi\n");
    __asm("sub ebx, ecx\n");
    __asm("sar ebx, 0x02\n");
    __asm("cmp ebx, edx\n");
    __asm("jae jump_point_4\n");
    __asm("mov dword ptr ds:[edi], eax\n");
    __asm("add edi, 0x04\n");
    __asm("mov dword ptr ds:[esi+0x10], edi\n");
    __asm("jmp jump_point_5\n");
    __asm("jump_point_4:\n");
    __asm("cmp ecx, edi\n");
    __asm("jbe jump_point_6\n");

    reimpl_func_3();

    __asm("jump_point_6:\n");
    __asm("mov eax, dword ptr ds:[esi]\n");
    __asm("push edi\n");
    __asm("push eax\n");
    __asm("lea eax, dword ptr [esp+0x1C]\n");
    __asm("push eax\n");
    __asm("lea ecx, dword ptr [esp+0x24]\n");
    __asm("push ecx\n");
    __asm("mov eax, esi\n");

    reimpl_func_4();

    __asm("jump_point_5:\n");
    __asm("mov ebx, dword ptr ss:[esp+0x34]\n");
    __asm("next_iter:\n");
    __asm("inc ebx\n");
    __asm("mov dword ptr ss:[esp+0x34], ebx\n");

    //effectively __asm("cmp ebx, [_g_subcateg_count+1]\n"); (to take "ALL SONGS" into account)
    __asm("push edx\n");
    __asm("mov edx, [_g_subcateg_count]\n");
    __asm("inc edx\n");
    __asm("cmp ebx, edx\n");
    __asm("pop edx\n");
    __asm("jb subcateg_loop\n");

    __asm("mov ecx, dword ptr ss:[esp+0x24]\n");
    __asm("mov dword ptr fs:[0], ecx\n");
    __asm("pop ecx\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ebp\n");
    __asm("pop ebx\n");
    __asm("add esp, 0x1C\n");
    __asm("ret 4\n");
}

//this replaces the category handling function ( add_song_in_list is a subroutine called by the game )
void categ_inject_songlist_simple()
{
    __asm("push ecx\n");
    __asm("push edx\n");
    songlist_struct.array_start = (uint32_t)songlist;
    songlist_struct.array_end = (uint32_t)&(songlist[songlist_count]);
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("push ecx\n");
    __asm("push _songlist_struct_addr\n");
    __asm("lea eax, dword ptr [ecx+0x24]\n");
    __asm("call [_add_song_in_list]\n");
    __asm("pop ecx\n");
}

void (*real_categ_reinit_songlist)();
void hook_categ_reinit_songlist()
{
    songlist_count = 0;
    real_categ_reinit_songlist();
}

void (*real_categ_build_songlist)();
void hook_categ_build_songlist()
{
    __asm("cmp eax, _g_min_id\n");
    __asm("jb categ_skip_add\n");
    __asm("cmp dword ptr _g_max_id, 0\n");
    __asm("je add_my_song\n");
    __asm("cmp eax, _g_max_id\n");
    __asm("ja categ_skip_add\n");

    __asm("add_my_song:\n");
    __asm("push eax\n");
    __asm("push ebx\n");
#if SIMPLE_CATEG_ALLOC == 1
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("sub esp, 0x08\n");
    songlist = (uint32_t*)realloc((void*)songlist, sizeof(uint32_t)*(songlist_count+4)); //TODO: only realloc when growing
    songlist_addr = (uint32_t)songlist;
    __asm("add esp, 0x08\n");
    __asm("pop edx\n");
    __asm("pop ecx\n");
#endif
    __asm("mov eax, [_songlist_count]\n");
    __asm("sal eax, 2\n");
    __asm("add eax, _songlist_addr\n");
    __asm("mov ebx, [edx]\n");
    __asm("mov dword ptr [eax], ebx\n");
    songlist_count++;
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("categ_skip_add:\n");
    real_categ_build_songlist();
}

void (*real_categ_printf_call)();
void (*real_categ_title_printf)();
void hook_categ_title_printf()
{
    __asm("cmp edi, 0x10\n");
    __asm("jle categ_title_printf_ok\n");
    __asm("mov eax, _g_categformat\n");
    __asm("push eax\n");
    __asm("jmp [_real_categ_printf_call]\n");
    __asm("categ_title_printf_ok:\n");
    real_categ_title_printf();
}

void (*real_categ_listing)();
void hook_categ_listing()
{
    __asm("cmp eax, 0x11\n");
    __asm("jne categ_listing_ok\n");
    __asm("cmp byte ptr ds:[_g_subcategmode], 1\n");
    __asm("jne skip_push\n");
    __asm("push esi\n"); //push esi to prepare for subcategory mode (categ_inject_songlist always points to the correct function)
    __asm("skip_push:\n");
    __asm("call [_categ_inject_songlist]\n"); //
    __asm("categ_listing_ok:\n");
    real_categ_listing();
}

static bool patch_custom_categ(const char *game_dll_fn) {

    DWORD dllSize = 0;
    char *data = getDllData(game_dll_fn, &dllSize);

    //patch format string for any category above 16 (prevent crash)
    {
        int64_t pattern_offset = search(data, dllSize, "\x6A\xFF\x8B\xCB\xFF\xD2\x50", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: custom_categ: cannot find category title format string function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x07;
        real_categ_printf_call = (void (*)())(patch_addr + 0x08);

        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_categ_title_printf,
                     (void **)&real_categ_title_printf);
    }

    // patch category handling jumptable to add our processing
    {
        int64_t pattern_offset = search(data, dllSize, "\x83\xF8\x10\x77\x75\xFF\x24\x85", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: custom_categ: cannot find category jump table\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x05 + 0x75;

        MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_categ_listing,
                     (void **)&real_categ_listing);

        if (g_subcategmode)
        {
            uint64_t function_call_addr = (int64_t)(data + pattern_offset + 0x05 + 69);
            uint32_t function_offset = *((uint32_t*)(function_call_addr +0x01));
            uint64_t function_addr = function_call_addr+5+function_offset;

            /* retrieve values from this function, needed for my reimplementation */
            reimpl_value_1 = *((uint32_t*)(function_addr +0x03));
            reimpl_value_2 = *((uint32_t*)(function_addr +0x16));
            reimpl_func_1                         = (void (*)())( *((uint32_t*)(function_addr +0x49)) + (uint32_t)(function_addr +0x04 +0x49) );
            reimpl_func_2_generate_event_category = (void (*)())( *((uint32_t*)(function_addr +0x73)) + (uint32_t)(function_addr +0x04 +0x73) );
            reimpl_func_3                         = (void (*)())( *((uint32_t*)(function_addr +0xBC)) + (uint32_t)(function_addr +0x04 +0xBC) );
            reimpl_func_4                         = (void (*)())( *((uint32_t*)(function_addr +0xD1)) + (uint32_t)(function_addr +0x04 +0xD1) );

            uint64_t patch_addr_2 = (int64_t)reimpl_func_2_generate_event_category + 80;
            //need to inject correct memory zone after generation as well
            MH_CreateHook((LPVOID)patch_addr_2, (LPVOID)hook_event_categ_generation,
                        (void **)&real_event_categ_generation);
        }
    }

    //add new category processing in jump table
    if (g_subcategmode)
    {
        categ_inject_songlist = &categ_inject_songlist_subcategs;
    }
    else
    {
        int64_t pattern_offset = search(data, dllSize, "\x8B\x4D\x10\x8B\x5D\x0C\x8B\xF1", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: custom_categ: cannot find add_song_in_list function\n");
            return false;
        }

        //I need to call this subfunction from my hook
        add_song_in_list = (void (*)())(data + pattern_offset - 0x12);

        categ_inject_songlist = &categ_inject_songlist_simple;
    }

    //generate songlist with all songs whose ID is above the threshold
    if (!g_subcategmode)
    {
        {
            int64_t pattern_offset = search(data, dllSize, "\x00\x8B\x56\x04\x0F\xB7\x02\xE8", 8, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: custom_categ: cannot find songlist processing table\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset + 0x07;

            MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_categ_build_songlist,
                        (void **)&real_categ_build_songlist);
        }

        //force rearm songlist creation so that it keeps working
        {
            int64_t pattern_offset = search(data, dllSize, "\x33\xC9\xB8\x12\x00\x00\x00\xBA", 8, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: custom_categ: cannot find category generation function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset;

            MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_categ_reinit_songlist,
                        (void **)&real_categ_reinit_songlist);
        }
    }

    //bump category count from 16 to 17
    if (!find_and_patch_hex(game_dll_fn, "\x83\xFE\x11\x0F\x82\x59\xFE\xFF\xFF", 9, 2, "\x12", 1))
    {
        return false;
    }

    //patch getCategName for a 17th entry
    {
        //make array one cell larger
        if (!find_and_patch_hex(game_dll_fn, "\x83\xEC\x44\x98\xC7\x04\x24", 7, 2, "\x48", 1))
        {
            return false;
        }
        //add the new name
        uint32_t categname_str_addr = (uint32_t)g_categname;
        char categname_patch_string[] = "\xC7\x44\x24\x44\x20\x00\x28\x10\x8B\x04\x84\x83\xC4\x48\xC3";
        memcpy(categname_patch_string+4, &categname_str_addr, 4);
        if (!find_and_patch_hex(game_dll_fn, "\x8B\x04\x84\x83\xC4\x44\xC3\xCC", 8, 0, categname_patch_string, 15))
        {
            return false;
        }
    }

    //patch getIconName for a 17th entry
    {
        if (!find_and_patch_hex(game_dll_fn, "\x83\xEC\x44\x98\xC7\x04\x24", 7, 2, "\x48", 1)) //2nd occurrence since first one just got patched
        {
            return false;
        }
        //add the new icon name
        uint32_t categicon_str_addr = (uint32_t)g_categicon;
        char categicon_patch_string[] = "\xC7\x44\x24\x44\xDC\x00\x28\x10\x8B\x04\x84\x83\xC4\x48\xC3";
        memcpy(categicon_patch_string+4, &categicon_str_addr, 4);
        if (!find_and_patch_hex(game_dll_fn, "\x8B\x04\x84\x83\xC4\x44\xC3\xCC", 8, 0, categicon_patch_string, 15))
        {
            return false;
        }
    }

    LOG("popnhax: custom %s injected (for songids ", g_subcategmode? "subcategories":"category");
    if (g_max_id)
        LOG("between %d and %d (inclusive))\n", g_min_id, g_max_id);
    else
        LOG("above %d (inclusive))\n", g_min_id);

    return true;
}

//extract folder name (cut "data_mods")
static char *get_folder_name(const char* path) {
    size_t len = (size_t)(strchr(path+10, '\\')-(path+10));
    char *categ_name = (char*) malloc(len+1);
    strncpy(categ_name, path+10, len);
    categ_name[len] = '\0';
    return categ_name;
}

static void parse_musicdb(const char *input_filename) {
    char *title = get_folder_name(input_filename);

    subcategory_s *subcateg = get_subcateg(title);
    if ( subcateg == NULL )
    {
        subcategories = (subcategory_s*)realloc(subcategories, sizeof(subcategory_s)*((++g_subcateg_count)+1));
        subcateg = &(subcategories[g_subcateg_count]);
        subcateg->name = strdup(title);
        subcateg->songlist = NULL;
        subcateg->size = 0;
    }

    property* musicdb = load_prop_file(input_filename);
    //iterate over all music id
    property_node *prop = NULL;
    if ((prop = property_search(musicdb, NULL, "/database/music"))) {
        for (; prop != NULL; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
            char idxStr[256] = {};
            property_node_refer(musicdb, prop, "id@", PROPERTY_TYPE_ATTR, idxStr,
                                sizeof(idxStr));
            uint32_t songid = atoi(idxStr);
            add_song_to_subcateg(songid, subcateg);
        }
    }
}

static void load_databases() {
    SearchFile s;
    g_subcateg_count = 0;
    subcategories = (subcategory_s*)realloc(subcategories, sizeof(subcategory_s)*(1));
    subcategories[0].name = strdup("ALL SONGS");
    subcategories[0].songlist = NULL;
    subcategories[0].size = 0;

    printf("musicdb search...\n");
    s.search("data_mods", "xml", true);
    auto result = s.getResult();

    for(uint16_t i=0;i<result.size();i++)
    {
        if ( strstr(result[i].c_str(), "musicdb") == NULL )
            continue;
        printf("(musicdb) Loading %s...\n", result[i].c_str());
        parse_musicdb(result[i].c_str());
    }

    for (uint32_t i = 0; i < g_subcateg_count+1; i++)
    {
        if (subcategories[i].size != 0)
            LOG(" %s (%d songs)", subcategories[i].name, subcategories[i].size);
    }
    LOG("\n");
}

bool patch_custom_categs(const char *dllFilename, uint8_t mode, uint16_t min, uint16_t max)
{
    g_min_id = min;
    g_max_id = max;
    #if SIMPLE_CATEG_ALLOC == 1
        songlist = (uint32_t*)calloc(1,5);
    #endif
    if (mode == 2)
    {
        g_subcategmode = true;
        load_databases();
    }

    return patch_custom_categ(dllFilename);
}