// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#include <vector>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "util/fuzzy_search.h"

#include "minhook/hde32.h"
#include "minhook/include/MinHook.h"

#include "popnhax/config.h"
#include "util/patch.h"
#include "util/xmlprop.hpp"
#include "xmlhelper.h"

#include "tableinfo.h"
#include "loader.h"

#include "SearchFile.h"

uint8_t *add_string(uint8_t *input);

struct popnhax_config config = {};

PSMAP_BEGIN(config_psmap, static)
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_unlocks,
                 "/popnhax/force_unlocks")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, unset_volume,
                 "/popnhax/unset_volume")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, event_mode, "/popnhax/event_mode")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, remove_timer,
                 "/popnhax/remove_timer")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, freeze_timer,
                 "/popnhax/freeze_timer")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, skip_tutorials,
                 "/popnhax/skip_tutorials")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, patch_db,
                 "/popnhax/patch_db")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_expansions,
                 "/popnhax/disable_expansions")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_redirection,
                 "/popnhax/disable_redirection")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, patch_xml_auto,
                 "/popnhax/patch_xml_auto")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, patch_xml_filename,
                 "/popnhax/patch_xml_filename")
PSMAP_END

enum BufferIndexes {
    MUSIC_TABLE_IDX = 0,
    CHART_TABLE_IDX,
    STYLE_TABLE_IDX,
    FLAVOR_TABLE_IDX,
    CHARA_TABLE_IDX,
};

typedef struct {
    uint64_t type;
    uint64_t method;
    uint64_t offset;
    uint64_t expectedValue;
    uint64_t size;
} UpdateOtherEntry;

typedef struct {
    uint64_t type;
    uint64_t offset;
    uint64_t size;
} UpdateBufferEntry;

typedef struct {
    uint64_t method;
    uint64_t offset;
} HookEntry;

const size_t LIMIT_TABLE_SIZE = 5;
uint64_t limit_table[LIMIT_TABLE_SIZE] = {0};
uint64_t new_limit_table[LIMIT_TABLE_SIZE] = {0};
uint64_t buffer_addrs[LIMIT_TABLE_SIZE] = {0};

uint8_t *new_buffer_addrs[LIMIT_TABLE_SIZE] = {NULL};

std::vector<UpdateBufferEntry*> buffer_offsets;
std::vector<UpdateOtherEntry*> other_offsets;
std::vector<HookEntry*> hook_offsets;

void (*real_omnimix_patch_jbx)();
void omnimix_patch_jbx() {
    __asm("mov al, 'X'\n");
    __asm("mov byte [edi+4], al\n");
    real_omnimix_patch_jbx();
}

void (*real_check_music_idx)();
extern "C" void check_music_idx();

extern "C" int8_t check_music_idx_handler(int32_t music_idx, int32_t chart_idx, int32_t result) {
    int8_t override_flag = get_chart_type_override(new_buffer_addrs[MUSIC_TABLE_IDX], music_idx & 0xffff, chart_idx & 0x0f);

    printf("music_idx: %d, result: %d, override_flag: %d\n", music_idx & 0xffff, result, override_flag);

    if (override_flag != -1) {
        return override_flag;
    }

    return (music_idx & 0xffff) > limit_table[CHART_TABLE_IDX] ? 1 : result;
}

asm(
".global _check_music_idx\n"
"_check_music_idx:\n"
"       call [_real_check_music_idx]\n"
"       movzx eax, al\n"
"       push eax\n"
"       push ebx\n"
"       push esi\n"
"       call _check_music_idx_handler\n"
"       add esp, 12\n"
"       ret\n"
);

void (*real_check_music_idx_usaneko)();
extern "C" void check_music_idx_usaneko();
extern "C" int8_t check_music_idx_handler_usaneko(int32_t result, int32_t chart_idx, int32_t music_idx) {
    return check_music_idx_handler(music_idx, chart_idx, result);
}

asm(
".global _check_music_idx_usaneko\n"
"_check_music_idx_usaneko:\n"
"       push edi\n"
"       push ebx\n"
"       cmp eax, 0x18\n"
"       setl al\n"
"       movzx eax, al\n"
"       push eax\n"
"       call _check_music_idx_handler_usaneko\n"
"       add esp, 12\n"
"       jmp [_real_check_music_idx_usaneko]\n"
);

void patch_string(const char *input_string, const char *new_string) {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    while (1) {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, input_string, strlen(input_string))

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
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

char *parse_patchdb(const char *input_filename, char *base_data) {
    const char *folder = "data_mods\\";
    char *input_filepath = (char*)calloc(strlen(input_filename) + strlen(folder) + 1, sizeof(char));

    sprintf(input_filepath, "%s%s", folder, input_filename);

    property* config_xml = load_prop_file(input_filepath);

    free(input_filepath);

    char *target = (char*)calloc(64, sizeof(char));
    property_node_refer(config_xml, property_search(config_xml, NULL, "/patches"), "target@", PROPERTY_TYPE_ATTR, target, 64);

    READ_U32(config_xml, NULL, "/patches/limits/music", limit_music)
    READ_U32(config_xml, NULL, "/patches/limits/chart", limit_chart)
    READ_U32(config_xml, NULL, "/patches/limits/style", limit_style)
    READ_U32(config_xml, NULL, "/patches/limits/flavor", limit_flavor)
    READ_U32(config_xml, NULL, "/patches/limits/chara", limit_chara)

    limit_table[MUSIC_TABLE_IDX] = limit_music;
    limit_table[CHART_TABLE_IDX] = limit_chart;
    limit_table[STYLE_TABLE_IDX] = limit_style;
    limit_table[FLAVOR_TABLE_IDX] = limit_flavor;
    limit_table[CHARA_TABLE_IDX] = limit_chara;

    READ_HEX(config_xml, NULL, "/patches/buffer_base_addrs/music", buffer_addrs[MUSIC_TABLE_IDX])
    buffer_addrs[MUSIC_TABLE_IDX] = buffer_addrs[MUSIC_TABLE_IDX] > 0 ? (uint64_t)base_data + (buffer_addrs[MUSIC_TABLE_IDX] - 0x10000000) : buffer_addrs[MUSIC_TABLE_IDX];

    READ_HEX(config_xml, NULL, "/patches/buffer_base_addrs/chart", buffer_addrs[CHART_TABLE_IDX])
    buffer_addrs[CHART_TABLE_IDX] = buffer_addrs[CHART_TABLE_IDX] > 0 ? (uint64_t)base_data + (buffer_addrs[CHART_TABLE_IDX] - 0x10000000) : buffer_addrs[CHART_TABLE_IDX];

    READ_HEX(config_xml, NULL, "/patches/buffer_base_addrs/style", buffer_addrs[STYLE_TABLE_IDX])
    buffer_addrs[STYLE_TABLE_IDX] = buffer_addrs[STYLE_TABLE_IDX] > 0 ? (uint64_t)base_data + (buffer_addrs[STYLE_TABLE_IDX] - 0x10000000) : buffer_addrs[STYLE_TABLE_IDX];

    READ_HEX(config_xml, NULL, "/patches/buffer_base_addrs/flavor", buffer_addrs[FLAVOR_TABLE_IDX])
    buffer_addrs[FLAVOR_TABLE_IDX] = buffer_addrs[FLAVOR_TABLE_IDX] > 0 ? (uint64_t)base_data + (buffer_addrs[FLAVOR_TABLE_IDX] - 0x10000000) : buffer_addrs[FLAVOR_TABLE_IDX];

    READ_HEX(config_xml, NULL, "/patches/buffer_base_addrs/chara", buffer_addrs[CHARA_TABLE_IDX])
    buffer_addrs[CHARA_TABLE_IDX] = buffer_addrs[CHARA_TABLE_IDX] > 0 ? (uint64_t)base_data + (buffer_addrs[CHARA_TABLE_IDX] - 0x10000000) : buffer_addrs[CHARA_TABLE_IDX];

    printf("limit music: %lld\n", limit_table[MUSIC_TABLE_IDX]);
    printf("limit chart: %lld\n", limit_table[CHART_TABLE_IDX]);
    printf("limit style: %lld\n", limit_table[STYLE_TABLE_IDX]);
    printf("limit flavor: %lld\n", limit_table[FLAVOR_TABLE_IDX]);
    printf("limit chara: %lld\n", limit_table[CHARA_TABLE_IDX]);

    printf("buffer music: %llx\n", buffer_addrs[MUSIC_TABLE_IDX]);
    printf("buffer chart: %llx\n", buffer_addrs[CHART_TABLE_IDX]);
    printf("buffer style: %llx\n", buffer_addrs[STYLE_TABLE_IDX]);
    printf("buffer flavor: %llx\n", buffer_addrs[FLAVOR_TABLE_IDX]);
    printf("buffer chara: %llx\n", buffer_addrs[CHARA_TABLE_IDX]);

    const char *types[LIMIT_TABLE_SIZE] = {"music", "chart", "style", "flavor", "chara"};

    // Read buffers_patch_addrs
    for (size_t i = 0; i < LIMIT_TABLE_SIZE; i++) {
        const char strbase[] = "/patches/buffers_patch_addrs";
        size_t search_str_len = strlen(strbase) + 1 + strlen(types[i]) + 1;
        char *search_str = (char*)calloc(search_str_len, sizeof(char));

        if (!search_str) {
            printf("Couldn't create buffer of size %d\n", search_str_len);
            exit(1);
        }

        sprintf(search_str, "%s/%s", strbase, types[i]);
        printf("search_str: %s\n", search_str);

        property_node* prop = NULL;
        if ((prop = property_search(config_xml, NULL, search_str))) {
            for (; prop != NULL; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
                uint64_t offset = 0;
                READ_HEX(config_xml, prop, "", offset)

                printf("offset ptr: %llx\n", offset);

                UpdateBufferEntry *buffer_offset = new UpdateBufferEntry();
                buffer_offset->type = i;
                buffer_offset->offset = offset;
                buffer_offsets.push_back(buffer_offset);
            }
        }
    }

    // Read other_patches
    for (size_t i = 0; i < LIMIT_TABLE_SIZE; i++) {
        const char strbase[] = "/patches/other_patches";
        size_t search_str_len = strlen(strbase) + 1 + strlen(types[i]) + 1;
        char *search_str = (char*)calloc(search_str_len, sizeof(char));

        if (!search_str) {
            printf("Couldn't create buffer of size %d\n", search_str_len);
            exit(1);
        }

        sprintf(search_str, "%s/%s", strbase, types[i]);
        printf("search_str: %s\n", search_str);

        property_node* prop = NULL;
        if ((prop = property_search(config_xml, NULL, search_str))) {
            for (; prop != NULL; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
                char methodStr[256] = {};
                property_node_refer(config_xml, prop, "method@", PROPERTY_TYPE_ATTR, methodStr, sizeof(methodStr));
                uint32_t method = atoi(methodStr);

                char expectedStr[256] = {};
                property_node_refer(config_xml, prop, "expected@", PROPERTY_TYPE_ATTR, expectedStr, sizeof(expectedStr));
                uint64_t expected = strtol((const char*)expectedStr, NULL, 16);

                char sizeStr[256] = {};
                property_node_refer(config_xml, prop, "size@", PROPERTY_TYPE_ATTR, sizeStr, sizeof(sizeStr));
                uint64_t size = strtol((const char*)sizeStr, NULL, 16);

                if (size == 0) {
                    // I can't think of any patches that require a different size than 4 bytes
                    size = 4;
                }

                uint64_t offset = 0;
                READ_HEX(config_xml, prop, "", offset)

                printf("offset ptr: %llx, method = %d, expected = %llx, size = %lld\n", offset, method, expected, size);

                UpdateOtherEntry *other_offset = new UpdateOtherEntry();
                other_offset->type = i;
                other_offset->method = method;
                other_offset->offset = offset;
                other_offset->expectedValue = expected;
                other_offset->size = size;
                other_offsets.push_back(other_offset);
            }
        }
    }

    // Read hook_addrs
    {
        property_node* prop = NULL;
        if ((prop = property_search(config_xml, NULL, "/patches/hook_addrs/offset"))) {
            for (; prop != NULL; prop = property_node_traversal(prop, TRAVERSE_NEXT_SEARCH_RESULT)) {
                char methodStr[256] = {};
                property_node_refer(config_xml, prop, "method@", PROPERTY_TYPE_ATTR, methodStr, sizeof(methodStr));
                uint32_t method = atoi(methodStr);

                uint64_t offset = 0;
                READ_HEX(config_xml, prop, "", offset)

                printf("offset ptr: %llx\n", offset);

                HookEntry *hook_offset = new HookEntry();
                hook_offset->offset = offset;
                hook_offset->method = method;
                hook_offsets.push_back(hook_offset);
            }
        }
    }

    return target;
}

static bool patch_database(bool force_unlocks) {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x8D\x44\x24\x10\x88\x4C\x24\x10\x88\x5C\x24\x11\x8D\x50\x01", 15)

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset != -1) {
            uint64_t patch_addr = (int64_t)data + pattern_offset;
            MH_CreateHook((LPVOID)patch_addr, (LPVOID)omnimix_patch_jbx,
                          (void **)&real_omnimix_patch_jbx);

            printf("popnhax: Patched X rev for omnimix\n");
        } else {
            printf("popnhax: Couldn't find rev patch\n");
        }
    }

    char *target;

    if (config.patch_xml_auto) {
        const char *filename = NULL;	
        SearchFile s;
        uint8_t *datecode = NULL;
        bool found = false;

        printf("pophax: auto detect patch file\n");
        property *config_xml = load_prop_file("prop/ea3-config.xml");
        READ_STR_OPT(config_xml, property_search(config_xml, NULL, "/ea3/soft"), "ext", datecode)
        free(config_xml);

        if (datecode == NULL) {
            printf("popnhax: (patch_xml_auto) failed to retrieve datecode from ea3-config. Please disable patch_xml_auto option and use patch_xml_filename to specify which file should be used.\n");
            return false;
        }

        printf("popnhax: (patch_xml_auto) datecode from ea3-config : %s\n", datecode);
        printf("popnhax: (patch_xml_auto) XML patch files search...\n");
        s.search("data_mods", "xml", false);
        auto result = s.getResult();

        printf("popnhax: (patch_xml_auto) found %d xml files in data_mods\n",result.size());
        for (uint16_t i=0; i<result.size(); i++) {
            filename = result[i].c_str()+10; // strip "data_mods\" since parsedb will prepend it...
            printf("%d : %s\n",i,filename);
            if (strstr(result[i].c_str(), (const char *)datecode) != NULL) {
                found = true;
                printf("popnhax: (patch_xml_auto) found matching datecode, end search\n");
                break;
            }
        }

        if (!found) {
            printf("popnhax: (patch_xml_auto) matching datecode not found, defaulting to latest patch file.\n");
        }
        
        printf("popnhax: (patch_xml_auto) using %s\n",filename);
        target = parse_patchdb(filename, data);

    } else {

        target = parse_patchdb(config.patch_xml_filename, data);

    }
    
    if (config.disable_redirection) {
        config.disable_expansions = true;
    }

    musichax_core_init(
        force_unlocks,
        !config.disable_expansions,
        !config.disable_redirection,
        target,

        data,

        limit_table[MUSIC_TABLE_IDX],
        &new_limit_table[MUSIC_TABLE_IDX],
        (char*)buffer_addrs[MUSIC_TABLE_IDX],
        &new_buffer_addrs[MUSIC_TABLE_IDX],

        limit_table[CHART_TABLE_IDX],
        &new_limit_table[CHART_TABLE_IDX],
        (char*)buffer_addrs[CHART_TABLE_IDX],
        &new_buffer_addrs[CHART_TABLE_IDX],

        limit_table[STYLE_TABLE_IDX],
        &new_limit_table[STYLE_TABLE_IDX],
        (char*)buffer_addrs[STYLE_TABLE_IDX],
        &new_buffer_addrs[STYLE_TABLE_IDX],

        limit_table[FLAVOR_TABLE_IDX],
        &new_limit_table[FLAVOR_TABLE_IDX],
        (char*)buffer_addrs[FLAVOR_TABLE_IDX],
        &new_buffer_addrs[FLAVOR_TABLE_IDX],

        limit_table[CHARA_TABLE_IDX],
        &new_limit_table[CHARA_TABLE_IDX],
        (char*)buffer_addrs[CHARA_TABLE_IDX],
        &new_buffer_addrs[CHARA_TABLE_IDX]
    );
    limit_table[STYLE_TABLE_IDX] = new_limit_table[STYLE_TABLE_IDX];

    if (config.disable_redirection) {
        printf("Redirection-related code is disabled, buffer address, buffer size and related patches will not be applied");
        return true;
    }

    // Patch buffers
    for (size_t i = 0; i < buffer_offsets.size(); i++) {
        if (buffer_offsets[i]->offset == 0 || buffer_addrs[buffer_offsets[i]->type] == 0) {
            continue;
        }

        // buffer_base_offsets is required because it will point to the beginning of all of the buffers to calculate offsets
        uint64_t patch_addr = (int64_t)data + (buffer_offsets[i]->offset - 0x10000000);
        uint64_t cur_addr = *(int32_t*)patch_addr;

        uint64_t mem_addr = (uint64_t)new_buffer_addrs[buffer_offsets[i]->type] + (cur_addr - buffer_addrs[buffer_offsets[i]->type]);

        printf("Patching %llx -> %llx @ %llx (%llx - %llx = %llx)\n", cur_addr, mem_addr, buffer_offsets[i]->offset, cur_addr, buffer_addrs[buffer_offsets[i]->type], cur_addr - buffer_addrs[buffer_offsets[i]->type]);

        patch_memory(patch_addr, (char*)&mem_addr, 4);
    }

    printf("popnhax: patched memory db locations\n");

    if (config.disable_expansions) {
        printf("Expansion-related code is disabled, buffer size and related patches will not be applied");
        return true;
    }

    for (size_t i = 0; i < other_offsets.size(); i++) {
        // Get current limit so it can be used for later calculations
        uint32_t cur_limit = limit_table[other_offsets[i]->type];
        uint32_t limit_diff = cur_limit < new_limit_table[other_offsets[i]->type]
            ? new_limit_table[other_offsets[i]->type] - cur_limit
            : 0;

        if (limit_diff != 0)
        {
            uint64_t cur_value = 0;

            if (other_offsets[i]->size == 1) {
                cur_value = *(uint8_t*)(data + (other_offsets[i]->offset - 0x10000000));
            } else if (other_offsets[i]->size == 2) {
                cur_value = *(uint16_t*)(data + (other_offsets[i]->offset - 0x10000000));
            } else if (other_offsets[i]->size == 4) {
                cur_value = *(uint32_t*)(data + (other_offsets[i]->offset - 0x10000000));
            } else if (other_offsets[i]->size == 8) {
                cur_value = *(uint64_t*)(data + (other_offsets[i]->offset - 0x10000000));
            }

            if (other_offsets[i]->method != 12 && cur_value != other_offsets[i]->expectedValue) {
                printf("ERROR! Expected %llx, found %llx @ %llx!\n", other_offsets[i]->expectedValue, cur_value, other_offsets[i]->offset);
                exit(1);
            }

            uint64_t value = cur_value;
            uint32_t patch_size = 4;
            switch (other_offsets[i]->method) {
                // Add limit_diff to value
                case 0:
                    value = cur_value + limit_diff;
                    break;

                // Add limit_diff * 4 to value
                case 1:
                    value = cur_value + (limit_diff * 4);
                    break;

                // Add limit diff * 6 * 4 (number of charts * 4?)
                case 2:
                    value = cur_value + ((limit_diff * 6) * 4);
                    break;

                // Add limit_diff * 6
                case 3:
                    value = cur_value + (limit_diff * 6);
                    break;

                // Add limit_diff * 0x90
                case 4:
                    value = cur_value + (limit_diff * 0x90);
                    break;

                // Add limit_diff * 0x48
                case 5:
                    value = cur_value + (limit_diff * 0x48);
                    break;

                // Add limit_diff * 0x120
                case 6:
                    value = cur_value + (limit_diff * 0x120);
                    break;

                // Add limit_diff * 0x440
                case 7:
                    value = cur_value + (limit_diff * 0x440);
                    break;

                // Add limit_diff * 0x0c
                case 8:
                    value = cur_value + (limit_diff * 0x0c);
                    break;

                // Add limit_diff * 0x3d0
                case 9:
                    value = cur_value + (limit_diff * 0x3d0);
                    break;

                // Add (limit_diff * 0x3d0) + (limit_diff * 0x9c)
                case 10:
                    value = cur_value + (limit_diff * 0x3d0) + (limit_diff * 0x9c);
                    break;

                // Add (limit_diff * 0x3d0) + (limit_diff * 0x9c) + (limit_diff * 0x2a0)
                case 11:
                    value = cur_value + (limit_diff * 0x3d0) + (limit_diff * 0x9c) + (limit_diff * 0x2a0);
                    break;

                // NOP
                case 12:
                    value = 0x9090909090909090;
                    patch_size = other_offsets[i]->size;
                    break;

                default:
                    printf("Unknown command: %lld\n", other_offsets[i]->method);
                    break;
            }

            if (value != cur_value) {
                uint64_t patch_addr = (int64_t)data + (other_offsets[i]->offset - 0x10000000);
                patch_memory(patch_addr, (char*)&value, patch_size);

                printf("Patched %llx: %llx -> %llx\n", other_offsets[i]->offset, cur_value, value);
            }
        }
    }

    HMODULE _moduleBase = GetModuleHandle("popn22.dll");
    for (size_t i = 0; i < hook_offsets.size(); i++) {
        switch (hook_offsets[i]->method) {
            case 0: {
                // Peace hook
                printf("Hooking %llx %p\n", hook_offsets[i]->offset, _moduleBase);
                MH_CreateHook((void*)((uint8_t*)_moduleBase + (hook_offsets[i]->offset - 0x10000000)), (void *)&check_music_idx, (void **)&real_check_music_idx);
                break;
            }

            case 1: {
                // Usaneko hook
                printf("Hooking %llx %p\n", hook_offsets[i]->offset, _moduleBase);
                uint8_t nops[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
                patch_memory((uint64_t)((uint8_t*)_moduleBase + (hook_offsets[i]->offset - 0x10000000) - 6), (char *)&nops, 6);
                MH_CreateHook((void*)((uint8_t*)_moduleBase + (hook_offsets[i]->offset - 0x10000000)), (void *)&check_music_idx_usaneko, (void **)&real_check_music_idx_usaneko);
                break;
            }

            default:
                printf("Unknown hook command: %lld\n", hook_offsets[i]->method);
                break;
        }
    }

    printf("popnhax: patched limit-related code\n");

    return true;
}

static bool patch_unset_volume() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    int64_t first_loc = 0;

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x04\x00\x81\xC4\x00\x01\x00\x00\xC3\xCC", 10)

        first_loc = find_block(data, dllSize, &task, 0);
        if (first_loc == -1) {
            return false;
        }
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x83", 1)

        int64_t pattern_offset = find_block(data, 0x10, &task, first_loc);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\xC3", 1);
    }

    printf("popnhax: windows volume untouched\n");

    return true;
}

static bool patch_event_mode() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0,
                   "\x8B\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC"
                   "\xCC\xCC\xC7",
                   17)

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x31\xC0\x40\xC3", 4);
    }

    printf("popnhax: event mode forced\n");

    return true;
}

static bool patch_remove_timer() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    int64_t first_loc = 0;

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x8B\xAC\x24\x68\x01", 5)

        first_loc = find_block(data, dllSize, &task, 0);
        if (first_loc == -1) {
            return false;
        }
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x0F", 1)

        int64_t pattern_offset = find_block(data, 0x15, &task, first_loc);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x90\xE9", 2);
    }

    printf("popnhax: timer removed\n");

    return true;
}

static bool patch_freeze_timer() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\xC7\x45\x38\x09\x00\x00\x00", 7)

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x90\x90\x90\x90\x90\x90\x90", 7);
    }

    printf("popnhax: timer frozen at 10 seconds remaining\n");

    return true;
}

static bool patch_skip_tutorials() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    int64_t first_loc = 0;

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\xFD\xFF\x5E\xC2\x04\x00\xE8", 7)

        first_loc = find_block(data, dllSize, &task, 0);
        if (first_loc == -1) {
            return false;
        }
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x74", 1)

        int64_t pattern_offset = find_block(data, 0x10, &task, first_loc);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\xEB", 1);
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x66\x85\xC0\x75\x5E\x6A", 6)

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x66\x85\xC0\xEB\x5E\x6A", 6);
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x00\x5F\x5E\x66\x83\xF8\x01\x75", 8)

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x00\x5F\x5E\x66\x83\xF8\x01\xEB", 8);
    }

    printf("popnhax: menu and long note tutorials skipped\n");

    return true;
}

bool force_unlock_deco_parts() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x83\xC4\x04\x83\x38\x00\x75\x22", 8)

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset == -1) {
        printf("popnhax: couldn't unlock deco parts\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x83\xC4\x04\x83\x38\x00\x90\x90", 8);
    }

    printf("popnhax: unlocked deco parts\n");

    return true;
}

bool force_unlock_songs() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    int music_unlocks = 0, chart_unlocks = 0;

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x69\xC0\xAC\x00\x00\x00\x8B\x80", 8) // 0xac here is the size of music_entry. May change in the future

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset == -1) {
            printf("popnhax: couldn't unlock songs and charts\n");
            return false;
        }

        uint32_t buffer_offset = *(uint32_t*)((int64_t)data + pattern_offset + task.blocks[0].data[0].length);
        buffer_offset -= 0x1c; // The difference between music_entry.mask and music_entry.fw_genre_ptr to put it at the beginning of the entry
        music_entry *entry = (music_entry*)buffer_offset;

        for (int32_t i = 0; ; i++) {
            // Detect end of table
            // Kind of iffy but I think it should work
            if (entry->charts[6] != 0 && entry->hold_flags[7] != 0) {
                // These should *probably* always be 0 but won't be after the table ends
                break;
            }

            if ((entry->mask & 0x08000000) != 0) {
                printf("[%04d] Unlocking %s\n", i, entry->title_ptr);
                music_unlocks++;
            }

            if ((entry->mask & 0x00000080) != 0) {
                printf("[%04d] Unlocking charts for %s\n", i, entry->title_ptr);
                chart_unlocks++;
            }

            if ((entry->mask & 0x08000080) != 0) {
                uint32_t new_mask = entry->mask & ~0x08000080;
                patch_memory((uint64_t)&entry->mask, (char *)&new_mask, 4);
            }

            entry++; // Move to the next music entry
        }
    }

    printf("popnhax: unlocked %d songs and %d charts\n", music_unlocks, chart_unlocks);

    return true;
}

bool force_unlock_charas() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    int chara_unlocks = 0;

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x98\x6B\xC0\x4C\x8B\x80", 6) // 0x4c here is the size of character_entry. May change in the future

        int64_t pattern_offset = find_block(data, dllSize, &task, 0);
        if (pattern_offset == -1) {
            printf("popnhax: couldn't unlock characters\n");
            return false;
        }

        uint32_t buffer_offset = *(uint32_t*)((int64_t)data + pattern_offset + task.blocks[0].data[0].length);
        buffer_offset -= 0x48; // The difference between character_entry.game_version and character_entry.chara_id_ptr to put it at the beginning of the entry
        character_entry *entry = (character_entry*)buffer_offset;

        for (int32_t i = 0; ; i++) {
            // Detect end of table
            // Kind of iffy but I think it should work
            if (entry->_pad1[0] != 0 || entry->_pad2[0] != 0 || entry->_pad2[1] != 0 || entry->_pad2[2] != 0) {
                // These should *probably* always be 0 but won't be after the table ends
                break;
            }

            uint32_t new_flags = entry->flags & ~3;

            if (new_flags != entry->flags && entry->disp_name_ptr != NULL && strlen((char*)entry->disp_name_ptr) > 0) {
                printf("Unlocking [%04d] %s... %08x -> %08x\n", i, entry->disp_name_ptr, entry->flags, new_flags);
                patch_memory((uint64_t)&entry->flags, (char *)&new_flags, sizeof(uint32_t));

                if ((entry->flavor_idx == 0 || entry->flavor_idx == -1)) {
                    int flavor_idx = 1;
                    patch_memory((uint64_t)&entry->flavor_idx, (char *)&flavor_idx, sizeof(uint32_t));
                    printf("Setting default flavor for chara id %d\n", i);
                }

                chara_unlocks++;
            }

            entry++; // Move to the next character entry
        }
    }

    printf("popnhax: unlocked %d characters\n", chara_unlocks);

    return true;
}

static bool patch_unlocks_offline() {
    DWORD dllSize = 0;
    char *data = getDllData("popn22.dll", &dllSize);

    int64_t first_loc = 0;

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x8B\xE5\x5D\xC2\x04\x00\xC3", 7)

        first_loc = find_block(data, dllSize, &task, 0);
        if (first_loc == -1) {
            printf("Couldn't find unlock loc 1\n");
            return false;
        }
    }

    int64_t second_loc = 0;
    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x00\x00\x50\x8B", 4)

        second_loc = find_block(data, 0x50, &task, first_loc);
        if (first_loc == -1) {
            printf("Couldn't find unlock loc 2\n");
            return false;
        }
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\x00\x00\x84\xC0\x74", 5)

        int64_t pattern_offset = find_block(data, 0x10, &task, second_loc);
        if (pattern_offset == -1) {
            printf("Couldn't find first song unlock\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x00\x00\x84\xC0\x90\x90", 6);
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\xFF\xFF\xA9\x06\x00\x00\x68\x74", 8)

        int64_t pattern_offset = find_block(data, 0xDD, &task, second_loc);
        if (pattern_offset == -1) {
            printf("Couldn't find second song unlock\n");
            return false;
        }
        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\xFF\xFF\xA9\x06\x00\x00\x68\xEB", 8);

        printf("popnhax: songs unlocked for offline\n");
    }

    {
        fuzzy_search_task task;

        FUZZY_START(task, 1)
        FUZZY_CODE(task, 0, "\xA9\x50\x01\x00\x00\x74", 6)

        int64_t pattern_offset = find_block_back(data, 0x50000, &task, second_loc);
        if (pattern_offset == -1) {
            printf("Couldn't find character unlock\n");
            return false;
        }
        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\xA9\x50\x01\x00\x00\xEB", 6);

        printf("popnhax: characters unlocked for offline\n");
    }

    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        if (MH_Initialize() != MH_OK) {
            printf("Failed to initialize minhook\n");
            exit(1);
            return TRUE;
        }

        _load_config("popnhax.xml", &config, config_psmap);

        if (config.unset_volume) {
            patch_unset_volume();
        }

        if (config.event_mode) {
            patch_event_mode();
        }

        if (config.remove_timer) {
            patch_remove_timer();
        }

        if (config.freeze_timer) {
            patch_freeze_timer();
        }

        if (config.skip_tutorials) {
            patch_skip_tutorials();
        }

        if (config.patch_db) {
            patch_database(config.force_unlocks);
        }

        if (config.force_unlocks) {
            if (!config.patch_db) {
                // Only unlock using these methods if it's not done directly through the database hooks
                force_unlock_songs();
                force_unlock_charas();
            }

            patch_unlocks_offline();
            force_unlock_deco_parts();
        }

        MH_EnableHook(MH_ALL_HOOKS);

        break;
    }

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
