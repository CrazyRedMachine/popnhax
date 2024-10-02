// clang-format off
#include <windows.h>
#include <process.h>
#include <psapi.h>
// clang-format on

#include <vector>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <io.h>
#include <fcntl.h>
#define access _access

#include "popnhax/config.h"
#include "util/membuf.h"
#include "util/log.h"
#include "util/patch.h"
#include "util/xmlprop.hpp"
#include "xmlhelper.h"
#include "translation.h"
#include "custom_categs.h"
#include "omnimix_patch.h"
#include "attract.h"
#include "tachi.h"

#include "tableinfo.h"
#include "loader.h"


#include "SearchFile.h"

#define PROGRAM_VERSION "2.3.dev"

const char *g_game_dll_fn = NULL;
char *g_config_fn   = NULL;
FILE *g_log_fp = NULL;


#define DEBUG 0

double g_multiplier = 1.;
DWORD (*real_timeGetTime)();
DWORD patch_timeGetTime()
{
 static DWORD last_real = 0;
 static DWORD cumul_offset = 0;

 if (last_real == 0)
 {
     last_real = real_timeGetTime()*g_multiplier;
     return last_real;
 }

 DWORD real = real_timeGetTime()*g_multiplier;
 DWORD elapsed = real-last_real;
 if (elapsed > 16) cumul_offset+= elapsed;

 last_real = real;
 return real - cumul_offset;

}

bool patch_get_time(double time_rate)
{
    if (time_rate == 0)
        return true;

    g_multiplier = time_rate;
    HMODULE hinstLib = GetModuleHandleA("winmm.dll");
    _MH_CreateHook((LPVOID)GetProcAddress(hinstLib, "timeGetTime"), (LPVOID)patch_timeGetTime,
                      (void **)&real_timeGetTime);

    LOG("popnhax: time multiplier: %f\n", time_rate);
    return true;
}

static void memdump(uint8_t* addr, uint8_t len)
{
    LOG("MEMDUMP  :\n");
    for (int i=0; i<len; i++)
    {
        LOG("%02x ", *addr);
        addr++;
        if ((i+1)%16 == 0)
            LOG("\n");
    }

}

uint8_t *add_string(uint8_t *input);

struct popnhax_config config = {};

PSMAP_BEGIN(config_psmap, static)
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, quick_boot,
                 "/popnhax/quick_boot")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, hidden_is_offset,
                 "/popnhax/hidden_is_offset")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, iidx_hard_gauge,
                 "/popnhax/iidx_hard_gauge")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, show_fast_slow,
                 "/popnhax/show_fast_slow")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, show_details,
                 "/popnhax/show_details")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, show_offset,
                 "/popnhax/show_offset")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, pfree,
                 "/popnhax/pfree")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, quick_retire,
                 "/popnhax/quick_retire")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, back_to_song_select,
                 "/popnhax/back_to_song_select")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, score_challenge,
                 "/popnhax/score_challenge")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_hd_timing,
                 "/popnhax/force_hd_timing")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, force_hd_resolution,
                 "/popnhax/force_hd_resolution")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_unlocks,
                 "/popnhax/force_unlocks")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, audio_source_fix,
                 "/popnhax/audio_source_fix")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, unset_volume,
                 "/popnhax/unset_volume")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, event_mode,
                 "/popnhax/event_mode")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, remove_timer,
                 "/popnhax/remove_timer")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, freeze_timer,
                 "/popnhax/freeze_timer")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, skip_tutorials,
                 "/popnhax/skip_tutorials")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_full_opt,
                 "/popnhax/force_full_opt")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, netvs_off,
                 "/popnhax/netvs_off")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, guidese_off,
                 "/popnhax/guidese_off")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, patch_db,
                 "/popnhax/patch_db")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_multiboot,
                 "/popnhax/disable_multiboot")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, force_patch_xml,
                 "/popnhax/force_patch_xml")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, patch_xml_dump,
                 "/popnhax/patch_xml_dump")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, practice_mode,
                 "/popnhax/practice_mode")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, force_datecode,
                 "/popnhax/force_datecode")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, network_datecode,
                 "/popnhax/network_datecode")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_keysounds,
                 "/popnhax/disable_keysounds")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, keysound_offset,
                 "/popnhax/keysound_offset")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, audio_offset,
                 "/popnhax/audio_offset")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, beam_brightness,
                 "/popnhax/beam_brightness")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, fps_uncap,
                 "/popnhax/fps_uncap")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_translation,
                 "/popnhax/disable_translation")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, enhanced_polling,
                 "/popnhax/enhanced_polling")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, debounce,
                 "/popnhax/debounce")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, enhanced_polling_stats,
                 "/popnhax/enhanced_polling_stats")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, enhanced_polling_priority,
                 "/popnhax/enhanced_polling_priority")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, hispeed_auto,
                 "/popnhax/hispeed_auto")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U16, struct popnhax_config, hispeed_default_bpm,
                 "/popnhax/hispeed_default_bpm")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_S8, struct popnhax_config, base_offset,
                 "/popnhax/base_offset")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, custom_categ,
                 "/popnhax/custom_categ")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, exclude_omni,
                 "/popnhax/exclude_omni")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, partial_entries,
                 "/popnhax/partial_entries")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U16, struct popnhax_config, custom_categ_min_songid,
                 "/popnhax/custom_categ_min_songid")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, custom_exclude_from_version,
                 "/popnhax/custom_exclude_from_version")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, custom_exclude_from_level,
                 "/popnhax/custom_exclude_from_level")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, custom_category_title,
                 "/popnhax/custom_category_title")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, custom_category_format,
                 "/popnhax/custom_category_format")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, custom_track_title_format,
                 "/popnhax/custom_track_title_format")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, custom_track_title_format2,
                 "/popnhax/custom_track_title_format2")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, local_favorites,
                 "/popnhax/local_favorites")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_STR, struct popnhax_config, local_favorites_path,
                 "/popnhax/local_favorites_path")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, tachi_scorehook,
                 "/popnhax/tachi_scorehook")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, tachi_rivals,
                 "/popnhax/tachi_rivals")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, tachi_scorehook_skip_omni,
                 "/popnhax/tachi_scorehook_skip_omni")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, ignore_music_limit,
                 "/popnhax/ignore_music_limit")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, high_framerate,
                 "/popnhax/high_framerate")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, high_framerate_limiter,
                 "/popnhax/high_framerate_limiter")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_U16, struct popnhax_config, high_framerate_fps,
                 "/popnhax/high_framerate_fps")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, autopin,
                 "/popnhax/autopin")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, attract_ex,
                 "/popnhax/attract_ex")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, attract_interactive,
                 "/popnhax/attract_interactive")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, attract_full,
                 "/popnhax/attract_full")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, attract_lights,
                 "/popnhax/attract_lights")
PSMAP_MEMBER_REQ(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, force_slow_timer,
                 "/popnhax/force_slow_timer")
/* removed options are now hidden as optional */
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_U8, struct popnhax_config, survival_gauge,
                 "/popnhax/survival_gauge", 0)
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, survival_iidx,
                 "/popnhax/survival_iidx", false)
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, survival_spicy,
                 "/popnhax/survival_spicy", false)
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_expansions,
                 "/popnhax/disable_expansions", false)
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, disable_redirection,
                 "/popnhax/disable_redirection", false)
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, translation_debug,
                 "/popnhax/translation_debug", false)
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_U16, struct popnhax_config, time_rate,
                 "/popnhax/time_rate", 0)
PSMAP_MEMBER_OPT(PSMAP_PROPERTY_TYPE_BOOL, struct popnhax_config, extended_debug,
                 "/popnhax/extended_debug", false)
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

char *g_datecode_override = NULL;
void (*real_asm_patch_datecode)();
void asm_patch_datecode() {
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov ecx,10\n");
    __asm("mov esi, %0\n": :"b"((int32_t)g_datecode_override));
    __asm("add edi, 6\n");
    __asm("rep movsb\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");
    real_asm_patch_datecode();
}

/* retrieve destination address when it checks for "soft/ext", then wait next iteration to overwrite */
uint8_t g_libavs_datecode_patch_state = 0;
char *datecode_property_ptr;
void (*real_asm_patch_datecode_libavs)();
void asm_patch_datecode_libavs() {
    if (g_libavs_datecode_patch_state == 2)
        __asm("jmp leave_datecode_patch\n");

    if (g_libavs_datecode_patch_state == 1)
    {
        __asm("push edx\n");
        __asm("push eax\n");
        __asm("push ecx\n");
        /* memcpy(datecode_property_ptr, g_datecode_override, 10);*/
        __asm("mov edx,_g_datecode_override\n");
        __asm("mov eax,_datecode_property_ptr\n");
        __asm("mov ecx,dword ptr ds:[edx]      \n");
        __asm("mov dword ptr ds:[eax],ecx      \n");
        __asm("mov ecx,dword ptr ds:[edx+4]    \n");
        __asm("mov dword ptr ds:[eax+4],ecx    \n");
        __asm("movzx edx,word ptr ds:[edx+8]   \n");
        __asm("mov word ptr ds:[eax+8],dx      \n");
        __asm("pop ecx\n");
        __asm("pop eax\n");
        __asm("pop edx\n");

        g_libavs_datecode_patch_state++;
        __asm("jmp leave_datecode_patch\n");
    }

    __asm("mov %0, [esp+0x38]\n":"=a"(datecode_property_ptr):);
    if ((uint32_t)datecode_property_ptr > 0x200000 && datecode_property_ptr[0] == 's'&& datecode_property_ptr[5] == 'e'&& datecode_property_ptr[7] == 't')
    {
        __asm("mov %0, ecx\n":"=c"(datecode_property_ptr):);
        g_libavs_datecode_patch_state++;
    }
    __asm("leave_datecode_patch:\n");
    real_asm_patch_datecode_libavs();
}

/* ----------- r2nk226 ----------- */
struct REC {
    uint32_t timestamp;
    uint8_t judge;
    uint8_t button;
    uint8_t flag;
    uint8_t pad[1];
    int32_t timing;
    uint32_t recid;
} ;
static REC *recbinArray_loaded = NULL;
static REC *recbinArray_writing = NULL;
//struct SRAN {
//    uint32_t chart_addr;
//    uint8_t button;
//    uint8_t pad[3];
//} ;
//static SRAN *s_list = NULL;
//static uint16_t s_count;
uint32_t player_option_offset = 0;
uint32_t **player_options_addr = 0;
uint32_t *button_addr = 0;
uint32_t **usbpad_addr = 0;
int16_t p_version = 0;
uint8_t speed = 5;
uint32_t new_speed = 100;
uint32_t rec_musicid = 0;
uint16_t rec_SPflags = 0;
uint32_t rec_options = 0;
uint8_t rec_hispeed = 10;
uint8_t spec = 0;
uint32_t judge_bar_func = 0;
uint32_t playsramsound_func = 0;
uint32_t date_func = 0;
//uint32_t noteque_func = 0;
uint32_t *p_note = 0;
uint32_t j_win_addr = 0;
uint32_t g_auto_flag = 0;
uint32_t *input_func = 0;
uint32_t *ran_func = 0;
uint32_t chartbase_addr = 0;
bool r_ran = false;
bool regul_flag = false;
bool is_resultscreen_flag = false;
bool disp = false;
bool use_sp_flag = false;
bool stop_input = true;
bool stop_recchange = true;
bool p_record = false;
bool find_recdata = false;
bool rec_reload = false;
bool recsavefile = false;
/* --------------------------------- */

void (*real_omnimix_patch_jbx)();
void omnimix_patch_jbx() {
    __asm("mov %0, byte ptr [edi+4]\n":"=a"(spec): :);

    __asm("mov al, 'X'\n");
    __asm("mov byte [edi+4], al\n");
    real_omnimix_patch_jbx();
}

/*
 * auto hi-speed
 */
bool g_longest_bpm_old_chart = false;
bool g_bypass_hispeed = false; //bypass target update for mystery bpm and soflan songs
bool g_mystery_bpm = false;
bool g_soflan_retry = false;
uint32_t g_new_songid = 0; // always contains the latest songid
uint32_t g_last_soflan_songid = 0; // compare with g_new_songid to see if g_soflan_retry_hispeed should apply
uint32_t g_hispeed = 0; // multiplier
uint32_t g_soflan_retry_hispeed = 0; //hispeed value that is temporary kept for quick retry on soflan songs
uint32_t g_hispeed_addr = 0;
uint32_t g_target_bpm = 0;
uint32_t g_default_bpm = 0; //used to rearm between credits
uint16_t g_low_bpm = 0;
uint16_t g_hi_bpm = 0;
uint16_t g_longest_bpm = 0;
uint16_t *g_base_bpm_ptr = &g_hi_bpm; //will point to g_low_bpm or g_longest_bpm according to mode
uint32_t g_low_bpm_ebp_offset = 0;
unsigned char *g_chart_addr = 0;

typedef struct chart_chunk_s {
    uint32_t timestamp;
    uint16_t operation;
    uint16_t data;
    uint32_t duration;
} chart_chunk_t;
#define CHART_OP_BPM 0x0445
#define CHART_OP_END 0x0645

typedef struct bpm_list_s {
    uint16_t bpm;
    uint32_t duration;
    struct bpm_list_s *next;
} bpm_list_t;

static uint32_t add_duration(bpm_list_t *list, uint16_t bpm, uint32_t duration)
{
    if (list->bpm == 0)
    {
        list->bpm = bpm;
        list->duration = duration;
        return duration;
    }

    while (list->next != NULL)
    {
        if (list->bpm == bpm)
        {
            list->duration += duration;
            return list->duration;
        }
        list = list->next;
    }
    //new entry
    bpm_list_t *entry = (bpm_list_t *)malloc(sizeof(bpm_list_t));
    entry->bpm = bpm;
    entry->duration = duration;
    entry->next = NULL;
    list->next = entry;

    return duration;
}

static void destroy_list(bpm_list_t *list)
{
    while (list != NULL)
    {
        bpm_list_t *tmp = list;
        list = list->next;
        free(tmp);
    }
}

/* the goal is to set g_longest_bpm to the most used bpm from the chart present in memory at address g_chart_addr
 */
void compute_longest_bpm(){
    chart_chunk_t* chunk = NULL;
    unsigned char *chart_ptr = g_chart_addr;
    int chunk_size = g_longest_bpm_old_chart? 8 : 12;
    uint32_t prev_timestamp = 0x64; //game adds 0x64 to every timestamp of a chart upon loading
    uint16_t prev_bpm = 0;

    bpm_list_t *list = (bpm_list_t *)malloc(sizeof(bpm_list_t));
    list->bpm = 0;
    list->duration = 0;
    list->next = NULL;

    uint32_t max_duration = 0;
    uint16_t longest_bpm = 0;
    do {
        chunk = (chart_chunk_t*) chart_ptr;

        if ( chunk->operation == CHART_OP_BPM || chunk->operation == CHART_OP_END )
        {
            if (prev_bpm)
            {
                uint32_t bpm_dur = add_duration(list, prev_bpm, chunk->timestamp - prev_timestamp); //will add to existing entry or create a new one if not present
                if (bpm_dur > max_duration)
                {
                    max_duration = bpm_dur;
                    longest_bpm = prev_bpm;
                }
            }
            prev_bpm = chunk->data;
            prev_timestamp = chunk->timestamp;
        }
        chart_ptr += chunk_size;
    } while ( chunk->operation != CHART_OP_END );

    destroy_list(list);
    g_longest_bpm = longest_bpm;
}

void (*real_rearm_hispeed)();
void hook_rearm_hispeed()
{
    __asm("push eax\n");
    __asm("mov eax, %0\n"::"m"(g_default_bpm):);
    __asm("mov %0, eax\n":"=m"(g_target_bpm): :);
    __asm("pop eax\n");
    real_rearm_hispeed();
}

void (*real_set_hispeed)();
void hook_set_hispeed()
{
    g_bypass_hispeed = false;
    __asm("mov %0, eax\n":"=r"(g_hispeed_addr): :);
    real_set_hispeed();
}

void (*real_retrieve_chart_addr)();
void hook_retrieve_chart_addr()
{
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("mov %0, esi\n":"=a"(g_chart_addr): :);
    __asm("call %0\n"::"r"(compute_longest_bpm));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");
    real_retrieve_chart_addr();
}
void (*real_retrieve_chart_addr_old)();
void hook_retrieve_chart_addr_old()
{
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("mov %0, edi\n":"=a"(g_chart_addr): :);
    __asm("call %0\n"::"r"(compute_longest_bpm));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");
    real_retrieve_chart_addr_old();
}

/*
 * This is called a lot of times: when arriving on option select, and when changing/navigating *any* option
 * I'm hooking here to set hi-speed to the target BPM
 */
double g_hispeed_double = 0;
void (*real_read_hispeed)();
void hook_read_hispeed()
{
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");

    __asm("mov ecx, ebp\n");
    __asm("add ecx, dword ptr [%0]\n"::"a"(&g_low_bpm_ebp_offset));

    __asm("sub ecx, 0x9B4\n");
    __asm __volatile__("mov %0, dword ptr [ecx]\n":"=a"(g_new_songid): :);
    __asm("add ecx, 0x9B4\n");

    __asm __volatile__("mov %0, word ptr [ecx]\n":"=a"(g_low_bpm): :);
    __asm("add ecx, 2\n");
    __asm __volatile__("mov %0, word ptr [ecx]\n":"=a"(g_hi_bpm): :);
    __asm("add ecx, 2\n");
    __asm __volatile__("mov %0, byte ptr [ecx]\n":"=a"(g_mystery_bpm): :);

    if (   g_soflan_retry
      && ( g_mystery_bpm || g_low_bpm != g_hi_bpm )
      &&   g_soflan_retry_hispeed
      && ( g_last_soflan_songid == g_new_songid ) )
    {
        g_hispeed = g_soflan_retry_hispeed;
        __asm("jmp apply_hispeed\n");
    }

    if ( g_bypass_hispeed || g_target_bpm == 0 ) //bypass for mystery BPM and soflan songs once hispeed has been manually changed (to avoid hi-speed being locked since target won't change)
    {
        __asm("jmp leave_read_hispeed\n");
    }

    g_hispeed_double = (double)g_target_bpm / (double)(*g_base_bpm_ptr/10.0);
    g_hispeed = (uint32_t)(g_hispeed_double+0.5); //rounding to nearest
    if (g_hispeed > 0x64) g_hispeed = 0x64;
    if (g_hispeed < 0x0A) g_hispeed = 0x0A;

    __asm("apply_hispeed:\n");
    __asm("and edi, 0xFFFF0000\n");                   //keep existing popkun and hidden status values
    __asm("or edi, dword ptr[%0]\n"::"m"(g_hispeed)); //fix hispeed initial display on option screen
    __asm("mov eax, dword ptr[%0]\n"::"m"(g_hispeed_addr));
    __asm("mov dword ptr[eax], edi\n");

    __asm("leave_read_hispeed:\n");
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    real_read_hispeed();
}

void (*real_increase_hispeed)();
void hook_increase_hispeed()
{
    __asm("push eax\n");
    __asm("push edx\n");
    __asm("push ecx\n");

    //increase hispeed
    __asm("movzx ecx, word ptr[edi]\n");
    __asm("inc ecx\n");
    __asm("cmp ecx, 0x65\n");
    __asm("jb skip_hispeed_rollover_high\n");
    __asm("mov ecx, 0x0A\n");
    __asm("skip_hispeed_rollover_high:\n");

    if ( g_mystery_bpm || g_low_bpm != g_hi_bpm )
    {
        __asm("mov %0, ecx\n":"=m"(g_soflan_retry_hispeed):);
        g_soflan_retry = false;
        g_bypass_hispeed = true;
        g_last_soflan_songid = g_new_songid&0xFFFF;
        __asm("jmp leave_increase_hispeed\n");
    }

    //compute resulting bpm using exact same formula as game (base bpm in eax, multiplier in ecx)
    __asm("movsx eax, %0\n"::"m"(g_hi_bpm));
    __asm("cwde\n");
    __asm("movsx ecx,cx\n");
    __asm("imul ecx,eax\n");
    __asm("mov eax, 0x66666667\n");
    __asm("imul ecx\n");
    __asm("sar edx,2\n");
    __asm("mov eax,edx\n");
    __asm("shr eax,0x1F\n");
    __asm("add eax,edx\n");

    __asm("mov %0, eax\n":"=m"(g_target_bpm): :);

    __asm("leave_increase_hispeed:\n");
    __asm("pop ecx\n");
    __asm("pop edx\n");
    __asm("pop eax\n");
    real_increase_hispeed();
}

void (*real_decrease_hispeed)();
void hook_decrease_hispeed()
{
    __asm("push eax\n");
    __asm("push edx\n");
    __asm("push ecx\n");

    //decrease hispeed
    __asm("movzx ecx, word ptr[edi]\n");
    __asm("dec ecx\n");
    __asm("cmp ecx, 0x0A\n");
    __asm("jge skip_hispeed_rollover_low\n");
    __asm("mov ecx, 0x64\n");
    __asm("skip_hispeed_rollover_low:\n");

    if ( g_mystery_bpm || g_low_bpm != g_hi_bpm )
    {
        __asm("mov %0, ecx\n":"=m"(g_soflan_retry_hispeed):);
        g_soflan_retry = false;
        g_bypass_hispeed = true;
        g_last_soflan_songid = g_new_songid&0xFFFF;
        __asm("jmp leave_decrease_hispeed\n");
    }

    //compute resulting bpm using exact same formula as game (base bpm in eax, multiplier in ecx)
    __asm("movsx eax, %0\n"::"m"(g_hi_bpm));
    __asm("cwde\n");
    __asm("movsx ecx,cx\n");
    __asm("imul ecx,eax\n");
    __asm("mov eax, 0x66666667\n");
    __asm("imul ecx\n");
    __asm("sar edx,2\n");
    __asm("mov eax,edx\n");
    __asm("shr eax,0x1F\n");
    __asm("add eax,edx\n");

    __asm("mov %0, eax\n":"=m"(g_target_bpm): :);

    __asm("leave_decrease_hispeed:\n");
    __asm("pop ecx\n");
    __asm("pop edx\n");
    __asm("pop eax\n");
    real_decrease_hispeed();
}

void (*real_leave_options)();
void retry_soflan_reset()
{
    g_soflan_retry = false;
    real_leave_options();
}

bool patch_hispeed_auto(uint8_t mode)
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    g_base_bpm_ptr = &g_hi_bpm;
    if (mode == 2)
        g_base_bpm_ptr = &g_low_bpm;
    else if (mode == 3)
        g_base_bpm_ptr = &g_longest_bpm;

    g_target_bpm = g_default_bpm;

    /* reset target to default bpm at the end of a credit */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\x10\x8B\xC8\x8B\x42\x28\xFF\xE0\xCC", 10, 0);
        if (pattern_offset == -1) {
            LOG("WARNING: popnhax: auto hi-speed: cannot find playerdata clean function\n");
            return false;
        } else {
            uint64_t patch_addr = (int64_t)data + pattern_offset;

            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_rearm_hispeed,
                          (void **)&real_rearm_hispeed);
        }
    }

    /* retrieve hi-speed address */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\x89\x0C\x07\x0F\xB6\x45\x04", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed address\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x04;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_set_hispeed,
                      (void **)&real_set_hispeed);
    }
    /* write new hispeed according to target bpm */
    {
        /* improve compatibility with newer games */
        int64_t pattern_offset = _search(data, dllSize, "\x0B\x00\x83\xC4\x04\xEB\x57\x8B\xBC\x24", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find chart BPM address offset\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x21;
        g_low_bpm_ebp_offset = *((uint16_t *)(patch_addr));

        if (g_low_bpm_ebp_offset != 0x0A1A && g_low_bpm_ebp_offset != 0x0A1E)
        {
            LOG("popnhax: auto hi-speed: WARNING: unexpected BPM address offset (%hu), might not work\n", g_low_bpm_ebp_offset);
        }

        pattern_offset = _search(data, dllSize, "\x98\x50\x66\x8B\x85", 5, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed apply address\n");
            return false;
        }

        patch_addr = (int64_t)data + pattern_offset - 0x07;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_read_hispeed,
                      (void **)&real_read_hispeed);
    }
    /* update target bpm on hispeed increase */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\xFF\x07\x0F\xB7\x07\x66\x83\xF8\x64", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed increase\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_increase_hispeed,
                      (void **)&real_increase_hispeed);
    }
    /* update target bpm on hispeed decrease */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\xFF\x0F\x0F\xB7\x07\x66\x83\xF8\x0A", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find hi-speed decrease\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_decrease_hispeed,
                      (void **)&real_decrease_hispeed);
    }

    /* set g_soflan_retry back to false when leaving options */
    {
        int8_t delta = 0;
        int64_t first_loc = _search(data, dllSize, "\x0A\x00\x00\x83\xC0\x04\xBF\x0C\x00\x00\x00\xE8", 12, 0);
        if (first_loc == -1) {
            LOG("popnhax: auto hi-speed: cannot retrieve option screen loop function\n");
            return false;
        }

        int64_t pattern_offset = _search(data, 1000, "\x33\xC9\x51\x50\x8B", 5, first_loc);
        if (pattern_offset == -1) {
            pattern_offset = _search(data, 1000, "\x74\x1A\x8B\x85\x14\x0A\x00\x00\x53\x6A", 10, first_loc); // popn28
            delta = 0x23;
            if (pattern_offset == -1)
            {
                LOG("popnhax: auto hi-speed: cannot retrieve option screen leave\n");
                return false;
            }
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + delta;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)retry_soflan_reset,
                      (void **)&real_leave_options);
    }

    /* compute longest bpm for mode 3 */
    if (mode == 3)
    {
        int64_t pattern_offset = _search(data, dllSize, "\x00\x00\x72\x05\xB9\xFF", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto hi-speed: cannot find chart address\n");
            return false;
        }

        /* detect if usaneko+ */
        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x0F;
        uint8_t check_byte = *((uint8_t *)(patch_addr + 1));

        if (check_byte == 0x04)
        {
            patch_addr += 9;
            g_longest_bpm_old_chart = true;
            LOG("popnhax: auto hi-speed: old game version\n");
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_retrieve_chart_addr_old,
                          (void **)&real_retrieve_chart_addr_old);
        }
        else
        {
            patch_addr += 11;
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_retrieve_chart_addr,
                          (void **)&real_retrieve_chart_addr);
        }
    }

    LOG("popnhax: auto hi-speed enabled");
    if (g_target_bpm != 0)
        LOG(" with default %hubpm", g_target_bpm);
    if (mode == 2)
        LOG(" (lower bpm target)");
    else if (mode == 3)
        LOG(" (longest bpm target)");
    else
        LOG(" (higher bpm target)");

    LOG("\n");

    return true;
}

/* dummy function to replace real one when not found */
bool is_normal_mode_best_effort(){
    return true;
}
bool (*popn22_is_normal_mode)() = is_normal_mode_best_effort;

uint32_t g_startsong_addr = 0;
uint32_t g_transition_addr = 0;
uint32_t g_stage_addr = 0;
uint32_t g_score_addr = 0;
bool g_pfree_mode = false;
bool g_end_session = false;
bool g_return_to_options = false;
bool g_return_to_song_select = false;
bool g_return_to_song_select_num9 = false;
void (*real_screen_transition)();
void quickexit_screen_transition()
{
    if (g_return_to_options)
    {
        __asm("mov dword ptr [edi+0x30], 0x1C\n");
        //flag is set back to false in the option select screen after score cleanup
    }
    else if (g_return_to_song_select)
    {
        __asm("mov dword ptr [edi+0x30], 0x17\n");

        __asm("push eax");
        __asm("call %0"::"a"(popn22_is_normal_mode));
        __asm("test al,al");
        __asm("pop eax");
        __asm("je skip_change_flag");

        if ( g_pfree_mode )
        {
            g_return_to_song_select = false;
        }
        //flag is set back to false in hook_stage_increment otherwise
    }

    __asm("skip_change_flag:");
    g_end_session = false;
    real_screen_transition();
}

void (*real_retrieve_score)();
void quickretry_retrieve_score()
{
    __asm("mov %0, esi\n":"=S"(g_score_addr): :);
    /* let's force E and fail medal for quick exit
     * (regular retire or end of song will overwrite)
     */
    uint8_t *clear_type = (uint8_t*)(g_score_addr+0x24);
    uint8_t *clear_rank = (uint8_t*)(g_score_addr+0x25);
    if (*clear_type == 0)
    {
        *clear_type = 1;
        *clear_rank = 1;
    }
    real_retrieve_score();
}

void quickexit_option_screen_cleanup()
{
    if (g_return_to_options)
    {
        /* we got here from quickretry, cleanup score */
        __asm("push ecx\n");
        __asm("push esi\n");
        __asm("push edi\n");
        __asm("mov edi, %0\n": :"D"(g_score_addr));
        __asm("mov esi, edi\n");
        __asm("add esi, 0x560\n");
        __asm("mov ecx, 0x98\n");
        __asm("rep movsd\n");
        __asm("pop edi\n");
        __asm("pop esi\n");
        __asm("pop ecx\n");
        g_return_to_options = false;
    }
}

uint8_t g_skip_options = 0;
uint32_t loadnew2dx_func;
uint32_t playgeneralsound_func;
char *g_system2dx_filepath;
uint32_t g_addr_icca;
void (*real_option_screen)();
void quickexit_option_screen()
{
    /* r2nk226
        for record reloading */
    if (rec_reload) {
        __asm("push eax\n");
        rec_reload = false;
        g_return_to_options = false;
        stop_recchange = true;
        **usbpad_addr = 0;
        __asm("pop eax\n");
        goto reload;
    }
    /* --------------------------------- */

    quickexit_option_screen_cleanup();

    __asm("push ebx\n");

    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 28\n"); // numpad8: 80 00 00 00
    __asm("cmp bl, 8\n");
    __asm("pop ebx\n");
    __asm("jne real_option_screen\n");

    /* numpad 8 is held, set a flag to rewrite transition pointer later */
reload:
    __asm("mov %0, 1\n":"=m"(g_skip_options):);

    __asm("real_option_screen:\n");
    real_option_screen();
}

void (*real_option_screen_apply_skip)();
void quickexit_option_screen_apply_skip()
{
    __asm("cmp byte ptr[_g_skip_options], 0\n");
    __asm("je real_option_screen_no_skip\n");

    /* flag is set, rewrite transition pointer */
    __asm("pop ecx\n");
    __asm("pop edx\n");
    __asm("xor edx, edx\n");
    __asm("mov ecx, %0\n": :"c"(g_startsong_addr));
    __asm("push edx\n");
    __asm("push ecx\n");
    __asm("mov %0, 0\n":"=m"(g_skip_options):);

    __asm("real_option_screen_no_skip:\n");
    real_option_screen_apply_skip();
}

uint32_t g_transition_offset = 0xA10;
/* because tsumtsum doesn't go through the second transition, try to recover from a messup here */
void (*real_option_screen_apply_skip_tsum)();
void quickexit_option_screen_apply_skip_tsum()
{
    __asm("cmp byte ptr[_g_skip_options], 0\n");
    __asm("je real_option_screen_no_skip_tsum\n");

    /* flag is set, rewrite transition pointer */
    __asm("push ebx\n");
    __asm("push ecx\n");
    __asm("mov ebx, ecx\n");
    __asm("add ebx, [_g_transition_offset]\n");
    __asm("mov ebx, [ebx]\n");
    __asm("add ebx, 8\n");
    __asm("mov ecx, %0\n": :"c"(g_startsong_addr));
    __asm("mov [ebx], ecx\n");
    __asm("pop ecx\n");
    __asm("pop ebx\n");
    __asm("mov %0, 0\n":"=m"(g_skip_options):);

    __asm("real_option_screen_no_skip_tsum:\n");
    real_option_screen_apply_skip_tsum();
}

uint8_t g_srambypass = 0;

void (*real_option_screen_later)();
void backtosongselect_option_screen()
{
    /* cannot use backtosongselect when not in normal mode */
    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je exit_back_select");

    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("jne exit_back_select\n");
    g_return_to_song_select = true;
    g_return_to_song_select_num9 = true;

/* reload correct .2dx file to get correct generalSounds */
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("push %0\n"::"m"(g_system2dx_filepath));
    __asm("call %0\n"::"D"(loadnew2dx_func));
    __asm("add esp, 4\n");
/* play exit sound */
    __asm("push 0\n");
    __asm("push 0x19\n");
    __asm("call %0\n"::"D"(playgeneralsound_func));
    __asm("add esp, 8\n");
/* set srambypass flag */
    __asm("mov %0, 1\n":"=m"(g_srambypass):);
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    __asm("exit_back_select:\n");

    real_option_screen_later();
}

void (*real_backtosongselect_herewego1)();
void backtosongselect_herewego1()
{
    __asm("cmp %0, 1\n"::"m"(g_srambypass));
    __asm("jne skip_disable_herewego1\n");
    __asm("mov %0, 0\n":"=m"(g_srambypass):);
    __asm("xor eax, eax\n");
    __asm("skip_disable_herewego1:\n");
    real_backtosongselect_herewego1();
}
void (*real_backtosongselect_herewego2)();
void backtosongselect_herewego2()
{
    __asm("cmp %0, 1\n"::"m"(g_srambypass));
    __asm("jne skip_disable_herewego2\n");
    __asm("mov %0, 0\n":"=m"(g_srambypass):);
    __asm("xor eax, eax\n");
    __asm("skip_disable_herewego2:\n");
    real_backtosongselect_herewego2();
}
void (*real_backtosongselect_herewego3)();
void backtosongselect_herewego3()
{
    __asm("cmp %0, 1\n"::"m"(g_srambypass));
    __asm("jne skip_disable_herewego3\n");
    __asm("mov %0, 0\n":"=m"(g_srambypass):);
    __asm("xor eax, eax\n");
    __asm("skip_disable_herewego3:\n");
    real_backtosongselect_herewego3();
}

void (*real_backtosongselect_option_screen_auto_leave)();
void backtosongselect_option_screen_auto_leave()
{
    if ( g_return_to_song_select_num9 )
    {
        g_return_to_song_select_num9 = false;
        __asm("mov al, 1\n");
    }
    real_backtosongselect_option_screen_auto_leave();
}

void (*real_option_screen_yellow)();
void backtosongselect_option_yellow()
{
    /* cannot use backtosongselect when not in normal mode */
    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je exit_back_select_yellow");

    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("jne exit_back_select_yellow\n");

    g_return_to_song_select = true;
    g_return_to_song_select_num9 = true;

/* reload correct .2dx file to get correct generalSounds */
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("push %0\n"::"m"(g_system2dx_filepath));
    __asm("call %0\n"::"D"(loadnew2dx_func));
    __asm("add esp, 4\n");
/* play exit sound */
    __asm("push 0\n");
    __asm("push 0x19\n");
    __asm("call %0\n"::"D"(playgeneralsound_func));
    __asm("add esp, 8\n");
/* set srambypass flag */
    __asm("mov %0, 1\n":"=m"(g_srambypass):);
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    __asm("exit_back_select_yellow:\n");
    real_option_screen_yellow();
}

uint32_t g_option_yellow_leave_addr = 0;
void (*real_backtosongselect_option_screen_yellow_auto_leave)();
void backtosongselect_option_screen_yellow_auto_leave()
{
    if ( g_return_to_song_select_num9 )
    {
        g_return_to_song_select_num9 = false;
        __asm("push %0\n": :"m"(g_option_yellow_leave_addr));
        __asm("ret\n");
    }
    real_backtosongselect_option_screen_yellow_auto_leave();
}

void get_recPlayoptions() {
    uint32_t *g_options_addr;
    uint32_t *g_musicid_addr;
    uint8_t *g_rechispeed_addr;
    uint8_t stage_no = 0;
    uint32_t option_offset = 0;

    stage_no = *(uint8_t*)(**player_options_addr +0x0E);
    option_offset = player_option_offset * stage_no;

    g_musicid_addr = (uint32_t*)(**player_options_addr +0x20 +option_offset);
    rec_musicid = *g_musicid_addr;

    g_options_addr = (uint32_t*)(**player_options_addr +0x34 +option_offset);
    rec_options = *g_options_addr;

    g_rechispeed_addr = (uint8_t*)(**player_options_addr +0x2A +option_offset);
    rec_hispeed = (uint8_t)*g_rechispeed_addr;

    rec_SPflags = (new_speed << 8) | ((uint8_t)regul_flag << 4) | speed;
}

void save_recSPflags() {
    uint32_t *g_coolgreat_addr;
    uint32_t *g_goodbad_addr;
    uint32_t val = 0;
    uint8_t stage_no = 0;
    uint32_t option_offset = 0;
    uint32_t size = LOWORD(recbinArray_writing[0].timestamp);

    stage_no = *(uint8_t*)(**player_options_addr +0x0E);
    option_offset = player_option_offset * stage_no;
    g_coolgreat_addr = (uint32_t*)(**player_options_addr +0x64 +option_offset);
    g_goodbad_addr = (uint32_t*)(**player_options_addr +0x68 +option_offset);

    if (p_version == 4) {
        rec_options ^= 0x01000000; // guidese_flag reverse
    }

    recbinArray_writing[0].timestamp |= ((uint32_t)rec_SPflags << 16);
    uint32_t count_check = *g_coolgreat_addr + *g_goodbad_addr;
    count_check = LOWORD(count_check) + HIWORD(count_check);
    if (count_check != size) {
        uint32_t check_cool = 0;
        uint32_t check_great = 0;
        uint32_t check_good = 0;
        uint32_t check_bad = 0;
        for (uint32_t i=0; i < size; i++) {
            if (recbinArray_writing[i+2].judge == 2) {
                check_cool++;
            } else if ((recbinArray_writing[i+2].judge == 3) || (recbinArray_writing[i+2].judge == 4)) {
                check_great++;
            } else if ((recbinArray_writing[i+2].judge == 5) || (recbinArray_writing[i+2].judge == 6)) {
                check_good++;
            } else if ((recbinArray_writing[i+2].judge == 1) || (recbinArray_writing[i+2].judge == 7) || (recbinArray_writing[i+2].judge == 8) || (recbinArray_writing[i+2].judge == 9)) {
                check_bad++;
            }
        }
        uint32_t cg = (check_great << 16) | check_cool;
        uint32_t gb = (check_bad << 16) | check_good;
        recbinArray_writing[0].recid = cg;
        recbinArray_writing[0].timing = gb;
        val = cg ^ gb ^ recbinArray_writing[0].timestamp;
    } else {
        recbinArray_writing[0].recid =  *g_coolgreat_addr;
        recbinArray_writing[0].timing = *g_goodbad_addr;
        val = *g_coolgreat_addr ^ *g_goodbad_addr ^ recbinArray_writing[0].timestamp;
    }

    val = ~(val ^ 0x672DE);
    recbinArray_writing[0].judge = LOBYTE(val);
    recbinArray_writing[0].button = (uint8_t)(LOWORD(val) >> 8);
    recbinArray_writing[0].flag = (uint8_t)(HIWORD(val) & 0xFF);
    recbinArray_writing[0].pad[0] = (uint8_t)(HIWORD(val) >> 8);
    recbinArray_writing[1].timestamp = rec_options;
    recbinArray_writing[1].judge = rec_hispeed;
    recbinArray_writing[1].button = (uint8_t)~recbinArray_writing[0].button;
    recbinArray_writing[1].flag = (uint8_t)~recbinArray_writing[0].flag;

    uint16_t *button_no = *(uint16_t **)button_addr;
    uint8_t bt00 = *button_no;
    uint32_t bt14 = 0;
    uint32_t bt58 = 0;
    uint32_t i = 0;
    do
    {
        button_no++;
        bt14 |= *button_no << (i*8);
        ++i;
    } while (i < 4);
    i=0;
    do
    {
        button_no++;
        bt58 |= *button_no << (i*8);
        ++i;
    } while (i < 4);

    printf("bt00(%02x) bt14(%08X) bt58(%08X)\n", bt00, bt14, bt58);

    recbinArray_writing[1].pad[0] = bt00;
    recbinArray_writing[1].timing = bt14;
    recbinArray_writing[1].recid = bt58;
    /* .rec file format
        random,timing,gauge,guide_se,speed,(00),(00),bt_0-bt_8
                                    */
    stop_input = false;
}

void (*real_option_screen_simple)();
void quickexit_option_screen_simple() {
    /* for record reloading */
    if (rec_reload) {
        rec_reload = false;
        g_return_to_options = false;
    /* rewrite transition pointer */
        //__asm("pop edi\n");
        __asm("pop ecx\n");
        __asm("pop edx\n");
        __asm("xor edx, edx\n");
        __asm("mov ecx, %0\n"::"c"(g_startsong_addr));
        __asm("push edx\n");
        __asm("push ecx\n");
        //__asm("push edi\n");
    }
    real_option_screen_simple();
}

void (*restore_op)();
void restore_playoptions() {
    **usbpad_addr = 1;
    if (!p_record) {
        uint32_t *restore_addr;

        uint8_t stage_no = *(uint8_t*)(**player_options_addr +0x0E);
        uint32_t option_offset = player_option_offset * stage_no;

        restore_addr = (uint32_t*)(**player_options_addr +0x34 +option_offset);
        *restore_addr = rec_options;

        #if DEBUG == 1
            LOG("popnhax: restore_addr is 0x%X\n", **player_options_addr);
        #endif
        /*
        __asm("push ebx\n");
        __asm("mov eax, [%0]\n"::"a"(*g_plop_addr));
        __asm("mov ebx, %0\n"::"b"(rec_options));
        __asm("mov byte ptr [eax+0x34], bl\n");
        __asm("pop ebx\n");
        */
        /*
        if (s_list != NULL) {
            free(s_list);
            s_list = NULL;
            LOG("popnhax: S-Random_list memory free.\n");
        }
        */
        //stop_recchange = true;
    //    if (!p_record) save_recSPflags();
    }
    restore_op();
}
/* --------------------------------- */


/*
numpad values:

   | <<24 <<28 <<16
---+---------------
 8 |   7    8    9
 4 |   4    5    6
 2 |   1    2    3
 1 |   0    00

 e.g. numpad 9 = 8<<16 = 00 08 00 00
      numpad 2 = 2<<28 = 20 00 00 00
*/
void (*real_game_loop)();
void quickexit_game_loop()
{
    /* ----------- r2nk226 ----------- */
    stop_input = false;
    /* --------------------------------- */

    __asm("push ebx\n");

    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");

    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("je leave_song\n");

    __asm("shr ebx, 12\n"); // (adds to the previous shr 16) numpad8: 08 00 00 00
    __asm("cmp bl, 8\n");
    __asm("jne call_real\n");

    /* numpad 8 is pressed: quick retry if pfree is active */
    use_sp_flag = false;

    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_pfree_check");

    if ( !g_pfree_mode )
    {
        __asm("skip_pfree_check:");
        __asm("jmp call_real\n");
    }
    else
    {
        if (g_mystery_bpm || g_low_bpm != g_hi_bpm)
            g_soflan_retry = true;
    }

    g_return_to_options = true;

    /* numpad 8 or 9 is pressed */
    __asm("leave_song:\n");
    __asm("mov eax, 1\n");
    __asm("pop ebx\n");
    __asm("ret\n");

    /* no quick exit */
    __asm("call_real:\n");
    __asm("pop ebx\n");

/* ----------- r2nk226 ----------- */
    if (regul_flag || r_ran || p_record) {
        use_sp_flag = true;
    }
    is_resultscreen_flag = false;
    disp = true;
/* --------------------------------- */
    real_game_loop();

}

int16_t g_keysound_offset = 0;
void (*real_eval_timing)();
void patch_eval_timing() {
    __asm("mov esi, %0\n": :"b"((int32_t)g_keysound_offset));
    real_eval_timing();
}

uint32_t g_last_playsram = 0;

void (*real_result_loop)();
void quickexit_result_loop()
{
    __asm("push ecx\n");
    __asm("mov ecx, %0\n": :"m"(g_addr_icca));
    __asm("mov ebx, [ecx]\n");
    __asm("pop ecx\n");

    __asm("add ebx, 0xAC\n");
    __asm("mov ebx, [ebx]\n");
    __asm("shr ebx, 16\n"); // numpad9: 00 08 00 00
    __asm("cmp bl, 8\n");
    __asm("je quit_session\n");

    __asm("shr ebx, 12\n"); // (adds to the previous shr 16) numpad8: 08 00 00 00
    __asm("cmp bl, 8\n");
    __asm("jne call_real_result\n");

    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_quickexit_pfree_check");

    if ( !g_pfree_mode )
    {
        __asm("skip_quickexit_pfree_check:");
        __asm("jmp call_real_result\n");
    }
    else
    {
        if (g_mystery_bpm || g_low_bpm != g_hi_bpm)
            g_soflan_retry = true;
    }

    g_return_to_options = true; //transition screen hook will catch it

    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call _timeGetTime@0\n");
    __asm("sub eax, [_g_last_playsram]\n");
    __asm("cmp eax, 10000\n"); //10sec cooldown
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("ja play_soundfx_retry\n"); //skip playing sound if cooldown value not reached
    __asm("pop eax\n");
    __asm("jmp call_real_result\n");

    __asm("play_soundfx_retry:\n");
    __asm("add [_g_last_playsram], eax"); // place curr_time in g_last_playsram (cancel sub eax, [_g_last_playsram])
    __asm("pop eax\n");
    /* play sound fx (retry song) */
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("mov eax, 0x16\n"); //"okay" sound fx
    __asm("push 0\n");
    __asm("call %0\n"::"D"(playsramsound_func));
    __asm("add esp, 4\n");
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    __asm("jmp call_real_result\n");

    __asm("quit_session:\n");
    g_end_session = true;
    g_return_to_options = false;
    /* set value 5 in g_stage_addr and -4 in g_transition_addr (to get fade to black transition) */
    __asm("mov ebx, %0\n": :"b"(g_stage_addr));
    __asm("mov dword ptr[ebx], 5\n");
    __asm("mov ebx, %0\n": :"b"(g_transition_addr));
    __asm("mov dword ptr[ebx], 0xFFFFFFFC\n"); //quit session

    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call _timeGetTime@0\n");
    __asm("sub eax, [_g_last_playsram]\n");
    __asm("cmp eax, 10000\n"); //10sec cooldown
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("ja play_soundfx_exit\n"); //skip playing sound if cooldown value not reached
    __asm("pop eax\n");
    __asm("jmp call_real_result\n");

    __asm("play_soundfx_exit:\n");
    __asm("add [_g_last_playsram], eax"); // place curr_time in g_last_playsram (cancel sub eax, [_g_last_playsram])
    __asm("pop eax\n");
    /* play sound fx (end session) */
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("mov eax, 0x16\n"); //"okay" sound fx
    __asm("push 0\n");
    __asm("call %0\n"::"D"(playsramsound_func));
    __asm("add esp, 4\n");
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("pop eax\n");

    /* ----------- r2nk226 ----------- */
    disp = false; // 8.9 message off
    speed = 5;
    new_speed = 100;
    stop_input = true;
    stop_recchange = true;

    __asm("call_real_result:\n");

    is_resultscreen_flag = true;
    /* --------------------------------- */
    real_result_loop();
}

void (*real_result_button_loop)();
void quickexit_result_button_loop()
{
    if ( g_end_session || g_return_to_options )
    {
        //g_return_to_options is reset in quickexit_option_screen_cleanup(), g_end_session is reset in quickexit_screen_transition()
        __asm("mov al, 1\n");
    }
    real_result_button_loop();
}

uint32_t g_timing_addr = 0;
bool g_timing_require_update = false;

void (*real_set_timing_func)();
void modded_set_timing_func()
{
    if (!g_timing_require_update)
    {
        real_set_timing_func();
        return;
    }

    /* timing contains offset, add to it instead of replace */
    __asm("push ebx\n");
    __asm("mov ebx, %0\n"::"m"(g_timing_addr));
    __asm("add [ebx], eax\n");
    __asm("pop ebx\n");

    g_timing_require_update = false;
    return;
}

volatile uint32_t g_masked_hidden = 0;

uint32_t g_show_hidden_addr = 0; /* offset from ESP at which hidden setting value is */
void (*real_show_hidden_result)();
void asm_show_hidden_result()
{
    if (g_masked_hidden)
    {
        __asm("push edx\n");
        __asm("mov edx, esp\n");
        __asm("add edx, %0\n"::""(g_show_hidden_addr):);
        __asm("add edx, 4\n"); /* to account for the "push edx" */
        __asm("or dword ptr [edx], 0x00000001\n");
        g_masked_hidden = 0;
        __asm("pop edx\n");
    }

    __asm("call_real_hidden_result:\n");
    real_show_hidden_result();
}

void (*real_stage_update)();
void hook_stage_update()
{
    __asm("mov ebx, dword ptr [esi+0x14]\n");
    __asm("lea ebx, [ebx+0xC]\n");
    __asm("mov %0, ebx\n":"=b"(g_transition_addr): :);

    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_stage_update_pfree_check");

    if ( !g_pfree_mode )
    {
        __asm("skip_stage_update_pfree_check:");
        real_stage_update();
    }
}

/* this hook is installed only when back_to_song_select is enabled and pfree is not */
void (*real_stage_increment)();
void hook_stage_increment()
{
    if ( !g_return_to_song_select )
        real_stage_increment();
    else
        g_return_to_song_select = false;
}

void (*real_check_music_idx)();
extern "C" void check_music_idx();

extern "C" int8_t check_music_idx_handler(int32_t music_idx, int32_t chart_idx, int32_t result) {
    int8_t override_flag = get_chart_type_override(new_buffer_addrs[MUSIC_TABLE_IDX], music_idx & 0xffff, chart_idx & 0x0f);

    LOG("music_idx: %d, result: %d, override_flag: %d\n", music_idx & 0xffff, result, override_flag);

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

char *parse_patchdb(const char *input_filename, char *base_data, membuf_t *membuf) {
    property* config_xml;

    if ( input_filename == NULL )
    {
        config_xml = load_prop_membuf(membuf);
    }
    else
    {
    const char *folder = "data_mods\\";
    char *input_filepath = (char*)calloc(strlen(input_filename) + strlen(folder) + 1, sizeof(char));

    sprintf(input_filepath, "%s%s", folder, input_filename);

    config_xml = load_prop_file(input_filepath);

    free(input_filepath);
    }

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

static bool patch_purelong()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize, "\x80\x1A\x06\x00\x83\xFA\x08\x77\x08", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Couldn't find score increment function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 24;
        uint8_t *patch_str = (uint8_t *) patch_addr;

        DWORD old_prot;
        VirtualProtect((LPVOID)patch_str, 20, PAGE_EXECUTE_READWRITE, &old_prot);
        /* replace cdq/idiv by xor/div in score increment computation to avoid score overflow */
        for (int i=12; i>=0; i--)
        {
            patch_str[i+1] = patch_str[i];
        }
        patch_str[0] = 0x33;
        patch_str[1] = 0xD2;
        patch_str[3] = 0x34;
        VirtualProtect((LPVOID)patch_str, 20, old_prot, &old_prot);
    }

    return true;
}


void (*real_normal0)();
void hook_normal0()
{
    // getChartDifficulty returns 0xFFFFFFFF when there's no chart,
    // but the game assumes there's always a normal chart so there's no check in this case
    // and it returns 0... let's fix this
    __asm("cmp ebx, 1\n"); //chart id
    __asm("jne process_normal0\n");
    __asm("cmp eax, 0\n"); //difficulty
    __asm("jne process_normal0\n");
    __asm("or eax, 0xFFFFFFFF\n");
    __asm("process_normal0:\n");
    real_normal0();
}

static bool patch_normal0()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize, "\x83\xC4\x08\x8B\xF8\x89\x7C\x24\x3C", 9, 0);
        if (pattern_offset == -1) {
            pattern_offset = _search(data, dllSize, "\x83\xC4\x08\x8B\xF8\x89\x7C\x24\x44", 9, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: normal0: Couldn't find song list display function\n");
                return false;
            }
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_normal0,
                      (void **)&real_normal0);

    }

    return true;
}

static bool get_music_limit(uint32_t* limit) {
    // avoid doing the search multiple times
    static uint32_t music_limit = 0;
    if ( music_limit != 0 )
    {
        *limit = music_limit;
        return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    PIMAGE_NT_HEADERS headers = (PIMAGE_NT_HEADERS)((int64_t)data + ((PIMAGE_DOS_HEADER)data)->e_lfanew);
    DWORD_PTR reloc_delta = (DWORD_PTR)((int64_t)data - headers->OptionalHeader.ImageBase);

    {
        int64_t string_loc = _search(data, dllSize, "Illegal music no %d", 19, 0);
        if (string_loc == -1) {
            LOG("popnhax: patch_db: could not retrieve music limit error string\n");
            return false;
        }

        string_loc += reloc_delta; //reloc delta just in case
        string_loc += 0x10000000; //entrypoint
        char *as_hex = (char *) &string_loc;

        int64_t pattern_offset = _search(data, dllSize, as_hex, 4, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: could not retrieve music limit test function\n");
            return false;
        }
        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x1F;
        *limit = *(uint32_t*)patch_addr;
        music_limit = *limit;
    }
    return true;
}

char *get_datecode_from_patches() {
        SearchFile s;
        char datecode[11] = {0};
        uint32_t music_limit = 0;
        if ( !get_music_limit(&music_limit) )
        {
            LOG("WARNING: could not retrieve music limit for datecode_auto\n");
            return NULL;
        }

        s.search("data_mods", "xml", false);
        auto result = s.getResult();

        for (uint16_t i=0; i<result.size(); i++) {
            uint32_t found_limit = 0;
            property *patch_xml = load_prop_file(result[i].c_str());
            READ_U32_OPT(patch_xml, property_search(patch_xml, NULL, "/patches/limits"), "music", found_limit);
            int res = property_node_refer(patch_xml, property_search(patch_xml, NULL, "/patches"), "target@",
                                PROPERTY_TYPE_ATTR, datecode, 11);
            free(patch_xml);
            if ( found_limit == music_limit )
                {
                    if ( res != 11 )
                    {
                        LOG("WARNING: %s did not contain a valid target, fallback to filename\n", result[i].c_str()+10);
                        memcpy(datecode, result[i].c_str()+10+8, 10);
                    }
                    return strdup(datecode);
                }
        }

        LOG("WARNING: datecode_auto: could not find matching patch file (patch file can still be generated)\n");
        return NULL;
}

static bool patch_datecode(char *datecode) {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    if ( strcmp(datecode, "auto") == 0 )
    {
        if ( !config.patch_db )
        {
            return true;
        }
        LOG("popnhax: find datecode based on patches_.xml file\n");
        g_datecode_override = get_datecode_from_patches();
        if ( g_datecode_override == NULL )
            return false;
    }
    else
        g_datecode_override = strdup(datecode);

    {
        int64_t pattern_offset = _search(data, dllSize, "\x8D\x44\x24\x10\x88\x4C\x24\x10\x88\x5C\x24\x11\x8D\x50\x01", 15, 0);
        if (pattern_offset != -1) {
            uint64_t patch_addr = (int64_t)data + pattern_offset + 0x08;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)asm_patch_datecode,
                          (void **)&real_asm_patch_datecode);

            LOG("popnhax: datecode set to %s",g_datecode_override);
        } else {
            LOG("popnhax: Couldn't patch datecode\n");
            return false;
        }
    }

    if (!config.network_datecode)
    {
        LOG("\n");
        return true;
    }

    /* network_datecode is on: also patch libavs so that forced datecode shows in network packets */
    DWORD avsdllSize = 0;
    char *avsdata = getDllData("libavs-win32.dll", &avsdllSize);
    {
        int64_t pattern_offset = _search(avsdata, avsdllSize, "\x57\x56\x89\x34\x24\x8B\xF2\x8B\xD0\x0F\xB6\x46\x2E", 13, 0);
        if (pattern_offset != -1) {
            uint64_t patch_addr = (int64_t)avsdata + pattern_offset;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)asm_patch_datecode_libavs,
                          (void **)&real_asm_patch_datecode_libavs);

            LOG(" (including network)\n");
        } else {
            LOG(" (WARNING: failed to apply to network)\n");
            return false;
        }
    }
    return true;
}

static bool patch_database() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    char *target;

    if ( strcmp(config.force_patch_xml, "") != 0 )
    {
        LOG("popnhax: patch_db: force patch file %s", config.force_patch_xml);
        target = parse_patchdb(config.force_patch_xml, data, NULL);
    }
    else
    {
        const char *filename = NULL;
        SearchFile s;
        uint8_t *datecode = NULL;
        bool found = false;
        uint32_t music_limit = 0;
        if ( !config.ignore_music_limit )
        {
            if ( !get_music_limit(&music_limit) ) {
                LOG("WARNING: could not retrieve music limit\n");
            } else {
                LOG("popnhax: patch_db: music limit : %d\n", music_limit);
            }
        } else {
                LOG("popnhax: patch_db: ignore music limit\n");
        }

        if ( g_datecode_override != NULL )
        {
            LOG("popnhax: patch_db: auto detect/generate patch file with datecode override\n");
            datecode = (uint8_t*) strdup(g_datecode_override);
        }
        else
        {
            LOG("popnhax: patch_db: auto detect/generate patch file with datecode from ea3-config\n");
            property *config_xml = load_prop_file("prop/ea3-config.xml");
            READ_STR_OPT(config_xml, property_search(config_xml, NULL, "/ea3/soft"), "ext", datecode)
            free(config_xml);
        }

        if (datecode == NULL) {
            LOG("popnhax: patch_db: failed to retrieve datecode. You can either rename popn22.dll to popn22_<datecode>.dll, fix ea3-config.xml, use force_datecode or use force_patch_xml.\n");
            exit(1);
        }

        LOG("popnhax: patch_db: datecode : %s\n", datecode);
        LOG("popnhax: patch_db: XML patch files search...\n");
        s.search("data_mods", "xml", false);
        auto result = s.getResult();

        LOG("popnhax: patch_db: found %d xml files in data_mods\n",result.size());
        for (uint16_t i=0; i<result.size(); i++) {

            filename = result[i].c_str()+10; // strip "data_mods\" since parsedb will prepend it...
            LOG("%d : %s\n",i,filename);
            bool datecode_match = (strstr(result[i].c_str(), (const char *)datecode) != NULL);
            bool limit_match = false;
            if ( music_limit != 0 )
            {
                uint32_t found_limit = 0;
                LOG("        check music limit... ");
                property *patch_xml = load_prop_file(result[i].c_str());
                READ_U32_OPT(patch_xml, property_search(patch_xml, NULL, "/patches/limits"), "music", found_limit)
                free(patch_xml);
                LOG("%d\n",found_limit);
                if ( found_limit == music_limit )
                {
                    limit_match = true;
                }
            }

            if ( limit_match )
            {
                LOG("popnhax: patch_db: found matching music limit, end search\n");
                if ( !datecode_match )
                {
                    LOG("WARNING: datecode did NOT match, please update your %s\n", ( g_datecode_override != NULL ) ? "force_datecode value" : "ea3-config.xml");
                }
                found = true;
                break;
            }

            if ( datecode_match )
            {
                if ( music_limit == 0 )
                {
                    found = true;
                    LOG("popnhax: patch_db: found matching datecode (no music limit check), end search\n");
                    break;
                }
                LOG("WARNING: found matching datecode but music limit doesn't check out, continue search\n");
            }
        }

        if (!found) {
            LOG("popnhax: patch_db: no matching patch file found, generating our own (datecode %s)\n", datecode);
            membuf_t *membuf = membuf_new(30000);
            if (membuf == NULL)
            {
                LOG("popnhax: patch_db: Failed to allocate membuf\n");
                exit(1);
            }

            make_omnimix_patch(g_game_dll_fn, membuf, (char*)datecode);
            membuf_rewind(membuf);

            if ( config.patch_xml_dump )
            {
                char out_filename[128];
                sprintf(out_filename, "data_mods\\patches_%s.xml", (char*)datecode);
                LOG("popnhax: patch_db: saving generated patches to %s\n", out_filename);
                membuf_tofile(membuf, out_filename);
            }

            target = parse_patchdb(NULL, data, membuf);
            membuf_free(membuf);
        } else {
            LOG("popnhax: patch_db: using %s\n",filename);
            target = parse_patchdb(filename, data, NULL);
        }
    }

    if (config.disable_redirection) {
        config.disable_expansions = true;
    }

    musichax_core_init(&config,
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

    patch_purelong();

    patch_normal0();

    {
        int64_t pattern_offset = _search(data, dllSize, "\x8D\x44\x24\x10\x88\x4C\x24\x10\x88\x5C\x24\x11\x8D\x50\x01", 15, 0);
        if (pattern_offset != -1) {
            uint64_t patch_addr = (int64_t)data + pattern_offset;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)omnimix_patch_jbx,
                          (void **)&real_omnimix_patch_jbx);

            LOG("popnhax: Patched X rev for omnimix\n");
        } else {
            LOG("popnhax: Couldn't find rev patch\n");
        }
    }

    if (config.disable_redirection) {
        LOG("Redirection-related code is disabled, buffer address, buffer size and related patches will not be applied");
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

    LOG("popnhax: patched memory db locations\n");

    if (config.disable_expansions) {
        printf("Expansion-related code is disabled, buffer size and related patches will not be applied");
        LOG("Expansion-related code is disabled, buffer size and related patches will not be applied");
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
                LOG("ERROR! Expected %llx, found %llx @ %llx!\n", other_offsets[i]->expectedValue, cur_value, other_offsets[i]->offset);
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

    HMODULE _moduleBase = GetModuleHandle(g_game_dll_fn);
    for (size_t i = 0; i < hook_offsets.size(); i++) {
        switch (hook_offsets[i]->method) {
            case 0: {
                // Peace hook
                printf("Hooking %llx %p\n", hook_offsets[i]->offset, _moduleBase);
                _MH_CreateHook((void*)((uint8_t*)_moduleBase + (hook_offsets[i]->offset - 0x10000000)), (void *)&check_music_idx, (void **)&real_check_music_idx);
                break;
            }

            case 1: {
                // Usaneko hook
                printf("Hooking %llx %p\n", hook_offsets[i]->offset, _moduleBase);
                uint8_t nops[] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
                patch_memory((uint64_t)((uint8_t*)_moduleBase + (hook_offsets[i]->offset - 0x10000000) - 6), (char *)&nops, 6);
                _MH_CreateHook((void*)((uint8_t*)_moduleBase + (hook_offsets[i]->offset - 0x10000000)), (void *)&check_music_idx_usaneko, (void **)&real_check_music_idx_usaneko);
                break;
            }

            default:
                printf("Unknown hook command: %lld\n", hook_offsets[i]->method);
                break;
        }
    }

    LOG("popnhax: patched limit-related code\n");

    return true;
}

static bool patch_audio_source_fix() {
    if (!find_and_patch_hex(g_game_dll_fn, "\x85\xC0\x75\x96\x8D\x70\x7F\xE8\xF8\x2B\x00\x00", 12, 0, "\x90\x90\x90\x90", 4))
    {
        return false;
    }

    LOG("popnhax: audio source fixed\n");

    return true;
}

static bool patch_unset_volume() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = _search(data, dllSize, "\x04\x00\x81\xC4\x00\x01\x00\x00\xC3\xCC", 10, 0);
    if (first_loc == -1) {
            return false;
    }

    int64_t pattern_offset = _search(data, 0x10, "\x83", 1, first_loc);
    if (pattern_offset == -1) {
        return false;
    }

    uint64_t patch_addr = (int64_t)data + pattern_offset;
    patch_memory(patch_addr, (char *)"\xC3", 1);

    LOG("popnhax: windows volume untouched\n");

    return true;
}

static bool patch_event_mode() {
    if (!find_and_patch_hex(g_game_dll_fn, "\x8B\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC"
                   "\xCC\xCC\xC7", 17, 0, "\x31\xC0\x40\xC3", 4))
    {
        return false;
    }

    LOG("popnhax: event mode forced\n");

    return true;
}

static bool patch_remove_timer() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = _search(data, dllSize, "\x8B\xAC\x24\x68\x01", 5, 0);
    if (first_loc == -1) {
        return false;
    }

    int64_t pattern_offset = _search(data, 0x15, "\x0F", 1, first_loc);
    if (pattern_offset == -1) {
        return false;
    }

    uint64_t patch_addr = (int64_t)data + pattern_offset;
    patch_memory(patch_addr, (char *)"\x90\xE9", 2);

    LOG("popnhax: timer removed\n");

    return true;
}

static bool patch_freeze_timer() {
    if (!find_and_patch_hex(g_game_dll_fn, "\xC7\x45\x38\x09\x00\x00\x00", 7, 0, "\x90\x90\x90\x90\x90\x90\x90", 7))
    {
        return false;
    }

    LOG("popnhax: timer frozen at 10 seconds remaining\n");

    return true;
}

static bool patch_skip_tutorials() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t first_loc = _search(data, dllSize, "\xFD\xFF\x5E\xC2\x04\x00\xE8", 7, 0);
        if (first_loc == -1) {
            return false;
        }

        int64_t pattern_offset = _search(data, 0x10, "\x84\xC0\x74", 3, first_loc);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 2;
        patch_memory(patch_addr, (char *)"\xEB", 1);
    }

    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\x85\xC0\x75\x5E\x6A", 6, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x66\x85\xC0\xEB\x5E\x6A", 6);
    }

    {
        int64_t pattern_offset = _search(data, dllSize, "\x00\x5F\x5E\x66\x83\xF8\x01\x75", 8, 0);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x00\x5F\x5E\x66\x83\xF8\x01\xEB", 8);
    }

    LOG("popnhax: menu and long note tutorials skipped\n");

    return true;
}

bool force_unlock_deco_parts() {
    if (!find_and_patch_hex(g_game_dll_fn, "\x83\xC4\x04\x83\x38\x00\x75\x22", 8, 6, "\x90\x90", 2))
    {
        LOG("popnhax: no deco parts to unlock\n");
        return false;
    }

    LOG("popnhax: unlocked deco parts\n");

    return true;
}

bool force_unlock_songs() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int music_unlocks = 0, chart_unlocks = 0;

    {
        // 0xac here is the size of music_entry. May change in the future
        int64_t pattern_offset = _search(data, dllSize, "\x69\xC0\xAC\x00\x00\x00\x8B\x80", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: couldn't unlock songs and charts\n");
            return false;
        }

        uint32_t buffer_offset = *(uint32_t*)((int64_t)data + pattern_offset + 8);
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
                LOG("[%04d] Unlocking %s\n", i, entry->title_ptr);
                music_unlocks++;
            }

            if ((entry->mask & 0x00000080) != 0) {
                LOG("[%04d] Unlocking charts for %s\n", i, entry->title_ptr);
                chart_unlocks++;
            }

            if ((entry->mask & 0x08000080) != 0) {
                uint32_t new_mask = entry->mask & ~0x08000080;
                patch_memory((uint64_t)&entry->mask, (char *)&new_mask, 4);
            }

            entry++; // Move to the next music entry
        }
    }

    LOG("popnhax: unlocked %d songs and %d charts\n", music_unlocks, chart_unlocks);

    return true;
}

bool force_unlock_charas() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int chara_unlocks = 0;

    {
        // 0x4c here is the size of character_entry. May change in the future
        int64_t pattern_offset = _search(data, dllSize, "\x98\x6B\xC0\x4C\x8B\x80", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: couldn't unlock characters\n");
            return false;
        }

        uint32_t buffer_offset = *(uint32_t*)((int64_t)data + pattern_offset + 6);
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
                LOG("Unlocking [%04d] %s... %08x -> %08x\n", i, entry->disp_name_ptr, entry->flags, new_flags);
                patch_memory((uint64_t)&entry->flags, (char *)&new_flags, sizeof(uint32_t));

                if ((entry->flavor_idx == 0 || entry->flavor_idx == -1)) {
                    int flavor_idx = 1;
                    patch_memory((uint64_t)&entry->flavor_idx, (char *)&flavor_idx, sizeof(uint32_t));
                    LOG("Setting default flavor for chara id %d\n", i);
                }

                chara_unlocks++;
            }

            entry++; // Move to the next character entry
        }
    }

    LOG("popnhax: unlocked %d characters\n", chara_unlocks);

    return true;
}

static bool patch_unlocks_offline() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize-0xE0000, "\xB8\x49\x06\x00\x00\x66\x3B", 7, 0xE0000);
        if (pattern_offset == -1) {
            LOG("Couldn't find first song unlock\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 2;
        patch_memory(patch_addr, (char *)"\x90\x90", 2);
    }

    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xA9\x06\x00\x00\x68\x74", 6, 5, "\xEB", 1))
        {
            LOG("Couldn't find second song unlock\n");
            return false;
        }
    }

    LOG("popnhax: songs unlocked for offline\n");

    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xA9\x10\x01\x00\x00\x74", 6, 5, "\xEB", 1)  /* unilab */
         && !find_and_patch_hex(g_game_dll_fn, "\xA9\x50\x01\x00\x00\x74", 6, 5, "\xEB", 1))
        {
            LOG("Couldn't find character unlock\n");
            return false;
        }

        LOG("popnhax: characters unlocked for offline\n");
    }

    return true;
}

/* helper function to retrieve numpad status address */
static bool get_addr_icca(uint32_t *res)
{
    static uint32_t addr = 0;

    if (addr != 0)
    {
            *res = addr;
            return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = _search(data, dllSize, "\xE8\x4B\x14\x00\x00\x84\xC0\x74\x03\x33\xC0\xC3\x8B\x0D", 14, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = *(uint32_t *) ((int64_t)data + pattern_offset + 14);
#if DEBUG == 1
    LOG("ICCA MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve timing offset global var address */
static bool get_addr_timing_offset(uint32_t *res)
{
    static uint32_t addr = 0;

    if (addr != 0)
    {
            *res = addr;
            return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = _search(data, dllSize, "\xB8\xB4\xFF\xFF\xFF", 5, 0);
    if (pattern_offset == -1) {
            return false;
    }

    uint32_t offset_delta = *(uint32_t *) ((int64_t)data + pattern_offset + 6);
    addr = *(uint32_t *) (((int64_t)data + pattern_offset + 10) + offset_delta + 1);
#if DEBUG == 1
    LOG("OFFSET MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve beam brightness address */
static bool get_addr_beam_brightness(uint32_t *res)
{
    static uint32_t addr = 0;

    if (addr != 0)
    {
            *res = addr;
            return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = _search(data, dllSize, "\xB8\x64\x00\x00\x00\xD9", 6, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = (uint32_t) ((int64_t)data + pattern_offset + 1);
#if DEBUG == 1
    LOG("BEAM BRIGHTNESS MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve SD timing address */
static bool get_addr_sd_timing(uint32_t *res)
{
    static uint32_t addr = 0;

    if (addr != 0)
    {
            *res = addr;
            return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = _search(data, dllSize, "\xB8\xC4\xFF\xFF\xFF", 5, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = (uint32_t) ((int64_t)data + pattern_offset + 1);
#if DEBUG == 1
    LOG("SD TIMING MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

/* helper function to retrieve HD timing address */
static bool get_addr_hd_timing(uint32_t *res)
{
    static uint32_t addr = 0;

    if (addr != 0)
    {
            *res = addr;
            return true;
    }

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t pattern_offset = _search(data, dllSize, "\xB8\xB4\xFF\xFF\xFF", 5, 0);
    if (pattern_offset == -1) {
        return false;
    }

    addr = (uint32_t) ((int64_t)data + pattern_offset + 1);
#if DEBUG == 1
    LOG("HD TIMING MEMORYZONE %x\n", addr);
#endif
    *res = addr;
    return true;
}

void (*real_commit_options_new)();
void hidden_is_offset_commit_options_new()
{
    /* game goes through this hook twice per commit, so we need to be careful */
    __asm("add eax, 0x2D\n"); //hidden value offset
    __asm("push ebx\n");
    __asm("movzx ebx, byte ptr [eax]\n");
    __asm("or [_g_masked_hidden], ebx\n"); /* save to restore display when using result_screen_show_offset patch */
    __asm("pop ebx\n");
    __asm("cmp byte ptr [eax], 1\n");
    __asm("jne call_real_commit_new\n");

    /* disable hidden ingame */
    __asm("mov byte ptr [eax], 0\n");

    /* flag timing for update */
    __asm("mov dword ptr [_g_timing_require_update], 1\n");

    /* write into timing offset */
    __asm("push ebx\n");
    __asm("push eax\n");
    __asm("movsx eax, word ptr [eax+1]\n");
    __asm("neg eax\n");
    __asm("mov ebx, %0\n"::"m"(g_timing_addr));
    __asm("mov [ebx], eax\n");
    __asm("pop eax\n");
    __asm("pop ebx\n");

    /* quit */
    __asm("call_real_commit_new:\n");
    __asm("sub eax, 0x2D\n");
    real_commit_options_new();
}

void (*real_commit_options)();
void hidden_is_offset_commit_options()
{
    __asm("push eax\n");
    /* check if hidden active */
    __asm("xor eax, eax\n");
    __asm("mov eax, [esi]\n");
    __asm("shr eax, 0x18\n");
    __asm("or %0, eax\n":"=m"(g_masked_hidden):); /* save to restore display when using result_screen_show_offset patch */
    __asm("cmp eax, 1\n");
    __asm("jne call_real_commit\n");

    /* disable hidden ingame */
    __asm("and ecx, 0x00FFFFFF\n"); // -kaimei
    __asm("and edx, 0x00FFFFFF\n"); //  unilab

    /* flag timing for update */
    __asm("mov %0, 1\n":"=m"(g_timing_require_update):);
    //g_timing_require_update = true;

    /* write into timing offset */
    __asm("push ebx\n");
    __asm("movsx eax, word ptr [esi+4]\n");
    __asm("neg eax\n");
    __asm("mov ebx, %0\n"::"m"(g_timing_addr));
    __asm("mov [ebx], eax\n");
    __asm("pop ebx\n");

    /* quit */
    __asm("call_real_commit:\n");
    __asm("pop eax\n");
    real_commit_options();
}

static bool patch_hidden_is_offset()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    if (!get_addr_timing_offset(&g_timing_addr))
    {
        LOG("popnhax: hidden is offset: cannot find timing offset address\n");
        return false;
    }

    /* patch option commit to store hidden value directly as offset */
    {
        uint8_t shift;
        int64_t pattern_offset;
        uint64_t patch_addr;

        if (config.game_version == 27)
        {
            shift = 6;
            pattern_offset = _search(data, dllSize, "\x03\xC7\x8D\x44\x01\x2A\x89\x10", 8, 0);
        }
        else if (config.game_version < 27)
        {
            pattern_offset = _search(data, dllSize, "\x0F\xB6\xC3\x03\xCF\x8D", 6, 0);
            shift = 14;
        }
        else
        {
            shift = 25;
            pattern_offset = _search(data, dllSize, "\x6B\xC9\x1A\x03\xC6\x8B\x74\x24\x10", 9, 0);
            patch_addr = (int64_t)data + pattern_offset + shift;

            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hidden_is_offset_commit_options_new,
                          (void **)&real_commit_options_new);
        }

        if (pattern_offset == -1) {
            LOG("popnhax: hidden is offset: cannot find address\n");
            return false;
        }

        patch_addr = (int64_t)data + pattern_offset + shift;

        if ( config.game_version <= 27 )
        {
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hidden_is_offset_commit_options,
                        (void **)&real_commit_options);
        }
    }

    /* turn "set offset" into an elaborate "sometimes add to offset" to use our hidden+ value as adjust */
    {
        char set_offset_fun[6] = "\xA3\x00\x00\x00\x00";
        uint32_t *cast_code = (uint32_t*) &set_offset_fun[1];
        *cast_code = g_timing_addr;

        int64_t pattern_offset = _search(data, dllSize, set_offset_fun, 5, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: hidden is offset: cannot find offset update function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)modded_set_timing_func,
                      (void **)&real_set_timing_func);

    }

    LOG("popnhax: hidden is offset: hidden is now an offset adjust\n");
    return true;
}

static bool patch_show_hidden_adjust_result_screen() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = _search(data, dllSize, "\x6A\x00\x0F\xBE\xCB", 5, 0);
    if (first_loc == -1)
        return false;


    int64_t pattern_offset = _search(data, 0x200, "\x80\xBC\x24", 3, first_loc);
    if (pattern_offset == -1) {
        return false;
    }
    g_show_hidden_addr = *((uint32_t *)((int64_t)data + pattern_offset + 0x03));
    uint64_t hook_addr = (int64_t)data + pattern_offset;
    _MH_CreateHook((LPVOID)(hook_addr), (LPVOID)asm_show_hidden_result,
                  (void **)&real_show_hidden_result);


    LOG("popnhax: show hidden/adjust value on result screen\n");

    return true;
}

static bool force_show_fast_slow() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = _search(data, dllSize, "\x6A\x00\x0F\xBE\xCB", 5, 0);
    if (first_loc == -1) {
        return false;
    }

    {
        int64_t pattern_offset = _search(data, 0x50, "\x0F\x85", 2, first_loc);
        if (pattern_offset == -1) {
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        patch_memory(patch_addr, (char *)"\x90\x90\x90\x90\x90\x90", 6);
    }

    LOG("popnhax: always show fast/slow on result screen\n");

    return true;
}


void (*real_show_detail_result)();
void hook_show_detail_result(){
    static uint32_t last_call = 0;

    __asm("push eax\n");
    __asm("push edx\n");

    uint32_t curr_time = timeGetTime();  //will clobber eax
    if ( curr_time - last_call > 10000 ) //will clobber edx
    {
        last_call = curr_time;
        __asm("pop edx\n");
        __asm("pop eax\n");
        //force press yellow button
        __asm("mov al, 1\n");
    }
    else
    {
        last_call = curr_time;
        __asm("pop edx\n");
        __asm("pop eax\n");
    }

    real_show_detail_result();
}

static bool force_show_details_result() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t first_loc = _search(data, dllSize, "\x8B\x45\x48\x8B\x58\x0C\x6A\x09\x68\x80\x00\x00\x00", 13, 0);
    if (first_loc == -1) {
        LOG("popnhax: show details: cannot find result screen button check (1)\n");
        return false;
    }

    {
        int64_t pattern_offset = _search(data, 0x50, "\x84\xC0", 2, first_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: show details: cannot find result screen button check (2)\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_show_detail_result,
                     (void **)&real_show_detail_result);
    }

    LOG("popnhax: force show details on result screen\n");

    return true;
}


uint8_t  g_pfree_song_offset   = 0x54;
uint16_t g_pfree_song_offset_2 = 0x558;
void (*popn22_get_powerpoints)();
void (*popn22_get_chart_level)();

/* POWER POINTS LIST FIX */
uint8_t   g_pplist_idx = 0;         // also serves as elem count
int32_t   g_pplist[20] = {0};       // 20 elements for power_point_list (always ordered)
int32_t   g_power_point_value = -1; // latest value (hook uses update_pplist() to add to g_pplist array)
int32_t  *g_real_pplist;            // list that the game retrieves from server
uint32_t *allocated_pplist_copy;    // pointer to the location where the game's pp_list modified copy resides

void pplist_reset()
{
        for (int i=0; i<20; i++)
            g_pplist[i] = -1;
        g_pplist_idx = 0;
}

/* add new value (stored in g_power_point_value) to g_pp_list */
void pplist_update(){
    if ( g_power_point_value == -1 )
        return;

    if ( g_pplist_idx == 20 )
    {
        for (int i = 0; i < 19; i++)
        {
            g_pplist[i] = g_pplist[i+1];
        }
        g_pplist_idx = 19;
    }

    g_pplist[g_pplist_idx++] = g_power_point_value;
    g_power_point_value = -1;
}

/* copy real pp_list to our local copy and check length */
void pplist_retrieve(){
    for (int i = 0; i < 20; i++)
    {
        g_pplist[i] = g_real_pplist[i];
    }

    /* in the general case your pplist will be full from the start so this is optimal */
    g_pplist_idx = 19;
    while ( g_pplist[g_pplist_idx] == -1 )
    {
        if ( g_pplist_idx == 0 )
            return;
        g_pplist_idx--;
    }
    g_pplist_idx++;
}

void (*real_pfree_pplist_init)();
void hook_pfree_pplist_init(){
    __asm("push eax");
    __asm("push ebx");
    __asm("lea ebx, [eax+0x1C4]\n");
    __asm("mov %0, ebx\n":"=m"(g_real_pplist));
    __asm("call %0\n"::"a"(pplist_retrieve));
    __asm("pop ebx");
    __asm("pop eax");
    real_pfree_pplist_init();
}

void (*real_pfree_pplist_inject)();
void hook_pfree_pplist_inject(){
    __asm("lea esi, %0\n"::"m"(g_pplist[g_pplist_idx]));
    __asm("mov dword ptr [esp+0x40], esi\n");

    __asm("lea esi, %0\n"::"m"(g_pplist));
    __asm("mov eax, dword ptr [esp+0x3C]\n");
    __asm("mov %0, eax\n":"=m"(allocated_pplist_copy));
    __asm("mov dword ptr [esp+0x3C], esi\n");
    __asm("movzx eax, %0\n"::"m"(g_pplist_idx));

    real_pfree_pplist_inject();
}

/* restore original pointer so that it can be freed */
void (*real_pfree_pplist_inject_cleanup)();
void hook_pfree_pplist_inject_cleanup()
{
    __asm("mov esi, %0\n"::"m"(allocated_pplist_copy));
    __asm("call %0\n"::"a"(pplist_reset));

    real_pfree_pplist_inject_cleanup();
}

/* hook is installed in stage increment function */
void (*real_pfree_cleanup)();
void hook_pfree_cleanup()
{
    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_pfree_cleanup");

    __asm("push esi\n");
    __asm("push edi\n");
    __asm("push eax\n");
    __asm("push edx\n");
    __asm("movzx eax, byte ptr [%0]\n"::"m"(g_pfree_song_offset));
    __asm("movzx ebx, word ptr [%0]\n"::"m"(g_pfree_song_offset_2));
    __asm("lea edi, dword ptr [esi+eax]\n");
    __asm("lea esi, dword ptr [esi+ebx]\n");
    __asm("push esi\n");
    __asm("push edi\n");

    /* compute powerpoints before cleanup */
    __asm("sub eax, 0x20\n"); // eax still contains g_pfree_song_offset
    __asm("neg eax\n");
    __asm("lea eax, dword ptr [edi+eax]\n");
    __asm("mov eax, dword ptr [eax]\n"); // music id (edi-0x38 or edi-0x34 depending on game)

    __asm("cmp ax, 0xBB8\n"); // skip if music id is >= 3000 (cs_omni and user customs)
    __asm("jae cleanup_score\n");

    __asm("push 0\n");
    __asm("push eax\n");
    __asm("shr eax, 0x10\n"); //sheet id in al
    __asm("call %0\n"::"b"(popn22_get_chart_level));
    __asm("add esp, 8\n");
    __asm("mov bl, byte ptr [edi+0x24]\n"); // medal

    /* push "is full combo" param */
    __asm("cmp bl, 8\n");
    __asm("setae dl\n");
    __asm("movzx ecx, dl\n");
    __asm("push ecx\n");

    /* push "is clear" param */
    __asm("cmp bl, 4\n");
    __asm("setae dl\n");
    __asm("movzx ecx, dl\n");
    __asm("push ecx\n");

    __asm("mov ecx, eax\n"); // diff level
    __asm("mov eax, dword ptr [edi]\n"); // score
    __asm("call %0\n"::"b"(popn22_get_powerpoints));
    __asm("mov %0, eax\n":"=a"(g_power_point_value):);
    __asm("call %0\n"::"a"(pplist_update));

    __asm("cleanup_score:\n");
    /* can finally cleanup score */
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("mov ecx, 0x98");
    __asm("rep movsd");
    __asm("pop edx");
    __asm("pop eax");
    __asm("pop edi");
    __asm("pop esi");
    __asm("jmp cleanup_end");

    __asm("skip_pfree_cleanup:\n");
    real_pfree_cleanup();

    __asm("cleanup_end:\n");
}

/* hook without the power point fixes (eclale best effort) */
void hook_pfree_cleanup_simple()
{
    __asm("push eax");
    __asm("call %0"::"a"(popn22_is_normal_mode));
    __asm("test al,al");
    __asm("pop eax");
    __asm("je skip_pfree_cleanup_simple");

    __asm("push esi\n");
    __asm("push edi\n");
    __asm("push eax\n");
    __asm("push ebx\n");
    __asm("push edx\n");
    __asm("movsx eax, byte ptr [%0]\n"::"m"(g_pfree_song_offset));
    __asm("movsx ebx, word ptr [%0]\n"::"m"(g_pfree_song_offset_2));
    __asm("lea edi, dword ptr [esi+eax]\n");
    __asm("lea esi, dword ptr [esi+ebx]\n");
    __asm("mov ecx, 0x98");
    __asm("rep movsd");
    __asm("pop edx");
    __asm("pop ebx");
    __asm("pop eax");
    __asm("pop edi");
    __asm("pop esi");
    __asm("ret");

    __asm("skip_pfree_cleanup_simple:\n");
    real_pfree_cleanup();
}

static bool patch_pfree() {
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    bool simple = false;
    pplist_reset();

    /* retrieve is_normal_mode function */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x83\xC4\x0C\x33\xC0\xC3\xCC\xCC\xCC\xCC\xE8", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: cannot find is_normal_mode function, fallback to best effort (active in all modes)\n");
        }
        else
        {
            popn22_is_normal_mode = (bool(*)()) (data + pattern_offset + 0x0A);
        }
    }

    /* stop stage counter (2 matches, 1st one is the good one) */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x83\xF8\x04\x77\x3E", 5, 0);
        if (pattern_offset == -1) {
            LOG("couldn't find stop stage counter\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x05;
        g_stage_addr = *(uint32_t*)(patch_addr+1);

        /* hook to retrieve address for exit to thank you for playing screen */
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_stage_update,
                     (void **)&real_stage_update);

    }

    /* retrieve memory zone parameters to prepare for cleanup */
    char offset_from_base = 0x00;
    char offset_from_stage1[2] = {0x00, 0x00};
    int64_t child_fun_loc = 0;
    {
        int64_t offset = _search(data, dllSize, "\x8D\x46\xFF\x83\xF8\x0A\x0F", 7, 0);
        if (offset == -1) {
        #if DEBUG == 1
            LOG("popnhax: pfree: failed to retrieve struct size and offset\n");
        #endif
            /* best effort for older games compatibility (works with eclale) */
            offset_from_base = 0x54;
            offset_from_stage1[0] = 0x04;
            offset_from_stage1[1] = 0x05;
            simple = true;
            goto pfree_apply;
        }
        uint32_t child_fun_rel = *(uint32_t *) ((int64_t)data + offset - 0x04);
        child_fun_loc = offset + child_fun_rel;
    }

    {
        int64_t pattern_offset = _search(data, 0x40, "\xCB\x69", 2, child_fun_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: failed to retrieve offset from stage1 (child_fun_loc = %llx\n",child_fun_loc);
            return false;
        }

        offset_from_stage1[0] = *(uint8_t *) ((int64_t)data + pattern_offset + 0x03);
        offset_from_stage1[1] = *(uint8_t *) ((int64_t)data + pattern_offset + 0x04);
        #if DEBUG == 1
            LOG("popnhax: pfree: offset_from_stage1 is %02x %02x\n",offset_from_stage1[0],offset_from_stage1[1]);
        #endif
    }

    {
        int64_t pattern_offset = _search(data, 0x40, "\x8d\x74\x01", 3, child_fun_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: pfree: failed to retrieve offset from base\n");
            return false;
        }

        offset_from_base = *(uint8_t *) ((int64_t)data + pattern_offset + 0x03);
        #if DEBUG == 1
            LOG("popnhax: pfree: offset_from_base is %02x\n",offset_from_base);
        #endif
    }

pfree_apply:
    g_pfree_song_offset    = offset_from_base;
    g_pfree_song_offset_2  = *((uint16_t*)offset_from_stage1);
    g_pfree_song_offset_2 += offset_from_base;

    /* cleanup score and stats */
    {
        int64_t pattern_offset = _search(data, dllSize, "\xFE\x46\x0E\x80", 4, 0);
        if (pattern_offset == -1) {
        LOG("popnhax: pfree: cannot find stage update function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        /* replace stage number increment with a score cleanup function */
        if ( simple )
        {
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_pfree_cleanup_simple,
                        (void **)&real_pfree_cleanup);
            LOG("popnhax: premium free enabled (WARN: no power points fix)\n");
            return true;
        }

        /* compute and save power points to g_pplist before cleaning up memory zone */
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_pfree_cleanup,
                     (void **)&real_pfree_cleanup);
    }

    /* fix power points */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8A\xD8\x8B\x44\x24\x0C\xE8", 7, 0);
        if (pattern_offset == -1) {
        LOG("popnhax: pfree: cannot find get_power_points function\n");
            return false;
        }

        popn22_get_chart_level = (void(*)()) (data + pattern_offset - 0x07);
    }

    {
        int64_t pattern_offset = _search(data, dllSize, "\x3D\x50\xC3\x00\x00\x7D\x05", 7, 0);
        if (pattern_offset == -1) {
        LOG("popnhax: pfree: cannot find get_power_points function\n");
            return false;
        }

        popn22_get_powerpoints = (void(*)()) (data + pattern_offset);
    }
    /* init pp_list */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x6B\xD2\x64\x2B\xCA\x51\x50\x68", 8, 0);
        if (pattern_offset == -1) {
        LOG("popnhax: pfree: cannot find power point load function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x1A;

        /* copy power point list to g_pplist on profile load and init g_pplist_idx */
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_pfree_pplist_init,
                     (void **)&real_pfree_pplist_init);
    }

    /* inject pp_list at end of credit */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\x74\x24\x3C\x66\x8B\x04\x9E", 8, 0);
        if (pattern_offset == -1) {
        LOG("popnhax: pfree: cannot find end of credit power point handling function (1)\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x07;

        /* make power point list pointers point to g_pplist at the end of processing */
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_pfree_pplist_inject,
                     (void **)&real_pfree_pplist_inject);
    }

    /* restore pp_list pointer so that it is freed at end of credit */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x7E\x04\x2B\xC1\x8B\xF8\x3B\xF5", 8, 0);
        if (pattern_offset == -1) {
        LOG("popnhax: pfree: cannot find end of credit power point handling function (2)\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x06;

        /* make power point list pointers point to g_pplist at the end of processing */
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_pfree_pplist_inject_cleanup,
                     (void **)&real_pfree_pplist_inject_cleanup);
    }

    LOG("popnhax: premium free enabled\n");

    return true;
}

static bool patch_quick_retire(bool pfree)
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    if ( !pfree )
    {
        /* pfree already installs this hook
         */
        {
            int64_t pattern_offset = _search(data, dllSize, "\x83\xF8\x04\x77\x3E", 5, 0);
            if (pattern_offset == -1) {
                LOG("couldn't find stop stage counter\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset - 0x05;
            g_stage_addr = *(uint32_t*)(patch_addr+1);

            /* hook to retrieve address for exit to thank you for playing screen */
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_stage_update,
                         (void **)&real_stage_update);

        }

        /* prevent stage number increment when going back to song select without pfree */
        if (config.back_to_song_select)
        {
            int64_t pattern_offset = _search(data, dllSize, "\xFE\x46\x0E\x80", 4, 0);
            if (pattern_offset == -1) {
            LOG("popnhax: quick retire: cannot find stage update function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset;
            /* hook to retrieve address for exit to thank you for playing screen */
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_stage_increment,
                         (void **)&real_stage_increment);
        }

        /* pfree already retrieves this function
         */
        {
            int64_t pattern_offset = _search(data, dllSize, "\x83\xC4\x0C\x33\xC0\xC3\xCC\xCC\xCC\xCC\xE8", 11, 0);
            if (pattern_offset == -1) {
                LOG("popnhax:  quick retire: cannot find is_normal_mode function, fallback to best effort (active in all modes)\n");
            }
            else
            {
                popn22_is_normal_mode = (bool(*)()) (data + pattern_offset + 0x0A);
            }
        }
    }

    /* instant retire with numpad 9 in song */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x55\x8B\xEC\x83\xE4\xF8\x83\xEC\x08\x0F\xBF\x05", 12, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: cannot retrieve song loop\n");
            return false;
        }

        if (!get_addr_icca(&g_addr_icca))
        {
            LOG("popnhax: cannot retrieve ICCA address for numpad hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_game_loop,
                      (void **)&real_game_loop);
    }

    {
        // PlaySramSound func
        int64_t pattern_offset = _search(data, dllSize,
                 "\x51\x56\x8B\xF0\x85\xF6\x74\x6C\x6B\xC0\x2C", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: PlaySramSound_addr was not found.\n");
            return false;
        }
        playsramsound_func = (uint32_t)((int64_t)data + pattern_offset);
    }

    /* instant exit with numpad 9 on result screen */
    {
        int64_t first_loc = _search(data, dllSize, "\xBF\x03\x00\x00\x00\x81\xC6", 7, 0);

        if (first_loc == -1) {
            LOG("popnhax: cannot retrieve result screen loop first loc\n");
            return false;
        }

        int64_t pattern_offset = _search(data, 0x50, "\x55\x8B\xEC\x83\xE4", 5, first_loc-0x50);
        if (pattern_offset == -1) {
            LOG("popnhax: cannot retrieve result screen loop\n");
            return false;
        }

        if (!get_addr_icca(&g_addr_icca))
        {
            LOG("popnhax: cannot retrieve ICCA address for numpad hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_result_loop,
                      (void **)&real_result_loop);
    }

    /* no need to press red button when numpad 8 or 9 is pressed on result screen */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x84\xC0\x75\x0F\x8B\x8D\x1C\x0A\x00\x00\xE8", 11, 0);
        int adjust = 0;

        if (pattern_offset == -1) {
            /* fallback */
            pattern_offset = _search(data, dllSize, "\x09\x00\x84\xC0\x75\x0F\x8B\x8D", 8, 0);
            adjust = 2;
            if (pattern_offset == -1) {
                LOG("popnhax: cannot retrieve result screen button check\n");
                return false;
            }
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x1A + adjust;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_result_button_loop,
                      (void **)&real_result_button_loop);

    }

    {
        /* retrieve current stage score addr for cleanup (also used to fix quick retire medal) */
        int64_t pattern_offset = _search(data, dllSize, "\xF3\xA5\x5F\x5E\x5B\xC2\x04\x00", 8, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retire: cannot retrieve score addr\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 5;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickretry_retrieve_score,
                      (void **)&real_retrieve_score);
    }

    LOG("popnhax: quick retire enabled\n");

    /* retrieve songstart function pointer for quick retry */
    {
        int64_t pattern_offset = _search(data, dllSize, "\xE9\x0C\x01\x00\x00\x8B\x85", 7, 0);
        int delta = -4;

        if (pattern_offset == -1) {
            delta = 18;
            pattern_offset = _search(data, dllSize, "\x6A\x00\xB8\x17\x00\x00\x00\xE8", 8, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: quick retry: cannot retrieve song start function\n");
                return false;
            }
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + delta;
        g_startsong_addr = *(uint32_t*)(patch_addr);
    }

    /* instant retry (go back to option select) with numpad 8 */
    {
        /* hook quick retire transition to go back to option select instead */
        int64_t pattern_offset = _search(data, dllSize, "\x8B\xE8\x8B\x47\x30\x83\xF8\x17", 8, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve screen transition function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_screen_transition,
                      (void **)&real_screen_transition);
    }

    /* instant launch song with numpad 8 on option select (hold 8 during song for quick retry)
     * there are now 3 patches: transition between song select and option select goes like A->B->C with C being the option select screen
     * because of special behavior, ac tsumtsum goes A->C so I cannot keep the patch in B else tsumtsum gets stuck in a never ending option select loop
     * former patch in B now sets a flag in A which is processed in B, then C also processes it in case the flag is still there */
    {
        int64_t pattern_offset = _search(data, dllSize, "\xE4\xF8\x51\x56\x8B\xF1\x80\xBE", 8, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve option screen loop\n");
            return false;
        }

        if (!get_addr_icca(&g_addr_icca))
        {
            LOG("popnhax: quick retry: cannot retrieve ICCA address for numpad hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x04;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_option_screen,
                      (void **)&real_option_screen);
    }
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\xF0\x83\x7E\x0C\x00\x0F\x84", 8, 0);

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve option screen loop\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x0F;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_option_screen_apply_skip,
                      (void **)&real_option_screen_apply_skip);
    }
    {
        int64_t pattern_offset = _search(data, dllSize, "\x0A\x00\x00\x83\x78\x34\x00\x75\x3D\xB8", 10, 0); //unilab
        uint8_t adjust = 15;
        g_transition_offset = 0xA10;
        if (pattern_offset == -1) {
            /* fallback */
            pattern_offset = _search(data, dllSize, "\x8B\x85\x0C\x0A\x00\x00\x83\x78\x34\x00\x75", 11, 0);
            adjust = 12;
            g_transition_offset = 0xA0C;
        }

        if (pattern_offset == -1) {
            LOG("popnhax: quick retry: cannot retrieve option screen loop function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - adjust;
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_option_screen_apply_skip_tsum,
                      (void **)&real_option_screen_apply_skip_tsum);
    }

    if (config.back_to_song_select)
    {
        char filepath[64];
        if (sprintf(filepath, "/data/sd/system/system%d/system%d.2dx", config.game_version, config.game_version) <= 0){
                LOG("popnhax: back to song select: cannot build systemxx.2dx filepath string.\n");
                return false;
        }
        g_system2dx_filepath = strdup(filepath);
        if (g_system2dx_filepath == NULL){
                LOG("popnhax: back to song select: cannot allocate systemxx.2dx filepath string.\n");
                return false;
        }

        {
            // loadnew2dx func
            int64_t pattern_offset = _search(data, dllSize,
                     "\x53\x55\x8B\x6C\x24\x0C\x56\x57\x8B\xCD", 10, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: loadnew2dx_addr was not found.\n");
                return false;
            }
            loadnew2dx_func = (uint32_t)((int64_t)data + pattern_offset);
        }

        {
            // playgeneralsound  func
            int64_t pattern_offset = _search(data, dllSize,
                     "\x33\xC0\x5B\xC3\xCC\xCC\xCC\xCC\xCC\x55\x8B\xEC\x83\xE4\xF8", 15, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: playgeneralsound_addr was not found.\n");
                return false;
            }
            playgeneralsound_func = (uint32_t)((int64_t)data + pattern_offset + 9);
        }

        /* prevent "here we go" sound from playing when going back to song select (3 occurrences) */
        {
            LPVOID hook[3] = { (LPVOID)backtosongselect_herewego1, (LPVOID)backtosongselect_herewego2, (LPVOID)backtosongselect_herewego3 };
            void** real[3] = { (void**)&real_backtosongselect_herewego1, (void**)&real_backtosongselect_herewego2, (void**)&real_backtosongselect_herewego3 };
            int64_t pattern_offset = 0;

            int i = 0;
            do {
                pattern_offset = _search(data, dllSize-pattern_offset-10, "\x6A\x00\xB8\x17\x00\x00\x00\xE8", 8, pattern_offset+10);
                if (pattern_offset == -1) {
                    LOG("popnhax: cannot find \"here we go\" sound play (occurrence %d).\n",i+1);
                } else {
                    uint64_t patch_addr = (int64_t)data + pattern_offset + 7;
                    _MH_CreateHook((LPVOID)patch_addr, hook[i],real[i]);
                    i++;
                }
            } while (i < 3 && pattern_offset != -1);

        }

        /* go back to song select with numpad 9 on song option screen (before pressing yellow) */
        {
            int64_t pattern_offset = -1;
            uint8_t adjust = 0;

            if (config.game_version < 27) {
                pattern_offset = _search(data, dllSize, "\x8B\x85\x0C\x0A\x00\x00\x83\x78\x34\x00\x75", 11, 0);
                adjust = 0;
            } else if (config.game_version == 27) {
                pattern_offset = _search(data, dllSize, "\x0A\x00\x00\x83\x78\x34\x00\x75\x3D\xB8", 10, 0);
                adjust = 3;
            } else { // let's hope for the future
                pattern_offset = _search(data, dllSize, "\x8B\x85\x10\x0A\x00\x00\x83\x78\x34\x00\x75", 11, 0);
                adjust = 0;
            }

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen loop function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset - adjust;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)backtosongselect_option_screen,
                          (void **)&real_option_screen_later);
        }
        /* automatically leave option screen after numpad 9 press */
        {
            int64_t pattern_offset = -1;
            uint8_t adjust = 0;
            if ( config.game_version <= 27 ) {
                pattern_offset = _search(data, dllSize, "\x0A\x00\x00\x83\xC0\x04\xBF\x0C\x00\x00\x00\xE8", 12, 0);
                adjust = 7;
            } else {
                pattern_offset = _search(data, dllSize, "\x84\xC0\x0F\x85\x91\x00\x00\x00\x8B", 9, 0);
                adjust = 0;
            }

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen loop function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset - adjust;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)backtosongselect_option_screen_auto_leave,
                          (void **)&real_backtosongselect_option_screen_auto_leave);
        }

        /* go back to song select with numpad 9 on song option screen (after pressing yellow) */
        {
            int64_t pattern_offset = _search(data, dllSize, "\x0A\x00\x00\x83\x78\x38\x00\x75\x3D\x68", 10, 0); //unilab
            uint8_t adjust = 3;
            if (pattern_offset == -1) {
                /* fallback */
                pattern_offset = _search(data, dllSize, "\x8B\x85\x0C\x0A\x00\x00\x83\x78\x38\x00\x75", 11, 0);
                adjust = 0;
            }

            if (pattern_offset == -1) {
                LOG("popnhax: quick retry: cannot retrieve yellow option screen loop function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset - adjust;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)backtosongselect_option_yellow,
                          (void **)&real_option_screen_yellow);
        }
        /* automatically leave after numpad 9 press */
        {
            int64_t pattern_offset = _search(data, dllSize, "\x8B\x55\x00\x8B\x82\x9C\x00\x00\x00\x6A\x01\x8B\xCD\xFF\xD0\x80\xBD", 17, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen yellow leave addr\n");
                return false;
            }

            g_option_yellow_leave_addr = (int32_t)data + pattern_offset - 0x05;
            pattern_offset = _search(data, dllSize, "\x84\xC0\x0F\x84\xF1\x00\x00\x00\x8B\xC5", 10, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: back to song select: cannot retrieve option screen yellow button check function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)backtosongselect_option_screen_yellow_auto_leave,
                          (void **)&real_backtosongselect_option_screen_yellow_auto_leave);
        }

        LOG("popnhax: quick retire: return to song select enabled\n");
    }

    if (pfree)
        LOG("popnhax: quick retry enabled\n");

    return true;
}

static bool patch_add_to_base_offset(int8_t delta) {
    int32_t new_value = delta;
    char *as_hex = (char *) &new_value;

    LOG("popnhax: base offset: adding %d to base offset.\n",delta);

    /* call get_addr_timing_offset() so that it can still work after timing value is overwritten */
    uint32_t original_timing;
    get_addr_timing_offset(&original_timing);

    uint32_t sd_timing_addr;
    if (!get_addr_sd_timing(&sd_timing_addr))
    {
        LOG("popnhax: base offset: cannot find base SD timing\n");
        return false;
    }

    int32_t current_value = *(int32_t *) sd_timing_addr;
    new_value = current_value+delta;
    patch_memory(sd_timing_addr, as_hex, 4);
    LOG("popnhax: base offset: SD offset is now %d.\n",new_value);


    uint32_t hd_timing_addr;
    if (!get_addr_hd_timing(&hd_timing_addr))
    {
        LOG("popnhax: base offset: cannot find base HD timing\n");
        return false;
    }
    current_value = *(int32_t *) hd_timing_addr;
    new_value = current_value+delta;
    patch_memory(hd_timing_addr, as_hex, 4);
    LOG("popnhax: base offset: HD offset is now %d.\n",new_value);

    return true;
}

bool g_hardware_offload = false;
bool g_enhanced_poll_ready = false;
int (*usbPadRead)(uint32_t*);

uint32_t g_poll_rate_avg = 0;
uint32_t g_last_button_state = 0;
uint8_t g_debounce = 0;
int32_t g_button_state[9] = {0};
static unsigned int __stdcall enhanced_polling_stats_proc(void *ctx)
{
    HMODULE hinstLib = GetModuleHandleA("ezusb.dll");
    usbPadRead = (int(*)(uint32_t*))GetProcAddress(hinstLib, "?usbPadRead@@YAHPAK@Z");
    static uint8_t button_debounce[9] = {0};

    for (int i=0; i<9; i++)
    {
        g_button_state[i] = -1;
    }

    while (!g_enhanced_poll_ready)
    {
        Sleep(500);
    }

    if (config.enhanced_polling_priority)
    {
        SetThreadPriority(GetCurrentThread(), config.enhanced_polling_priority);
        LOG("[Enhanced polling] Thread priority set to %d\n", GetThreadPriority(GetCurrentThread()));
    }

    uint32_t count = 0;
    uint32_t count_time = 0;

    uint32_t curr_poll_time = 0;
    uint32_t prev_poll_time = 0;
    while (g_enhanced_poll_ready)
    {
        uint32_t pad_bits;
        /* ensure at least 1ms has elapsed between polls
         * (beware of SD cab hardware compatibility)
         */
        curr_poll_time = timeGetTime();
        if (curr_poll_time == prev_poll_time)
        {
            curr_poll_time++;
            Sleep(1);
        }
        prev_poll_time = curr_poll_time;

        if (count == 0)
        {
            count_time = curr_poll_time;
        }

        usbPadRead(&pad_bits);
        g_last_button_state = pad_bits;

        unsigned int buttonState = (g_last_button_state >> 8) & 0x1FF;
        for (int i = 0; i < 9; i++)
        {
            if ( ((buttonState >> i)&1) )
            {
                if (g_button_state[i] == -1)
                {
                    g_button_state[i] = curr_poll_time;
                }
                //debounce on release (since we're forced to stub usbPadReadLast)
                button_debounce[i] = g_debounce;
            }
            else
            {
                if (button_debounce[i]>0)
                {
                    button_debounce[i]--;
                }

                if (button_debounce[i] == 0)
                {
                    g_button_state[i] = -1;
                }
                else
                {
                    //debounce ongoing: flag button as still pressed
                    g_last_button_state |= 1<<(8+i);
                }
            }
        }
        count++;
        if (count == 100)
        {
            g_poll_rate_avg = timeGetTime() - count_time;
            count = 0;
        }
    }
    return 0;
}

static unsigned int __stdcall enhanced_polling_proc(void *ctx)
{
    HMODULE hinstLib = GetModuleHandleA("ezusb.dll");
    usbPadRead = (int(*)(uint32_t*))GetProcAddress(hinstLib, "?usbPadRead@@YAHPAK@Z");
    static uint8_t button_debounce[9] = {0};

    for (int i=0; i<9; i++)
    {
        g_button_state[i] = -1;
    }

    while (!g_enhanced_poll_ready)
    {
        Sleep(500);
    }

    if (config.enhanced_polling_priority)
    {
        SetThreadPriority(GetCurrentThread(), config.enhanced_polling_priority);
        LOG("[Enhanced polling] Thread priority set to %d\n", GetThreadPriority(GetCurrentThread()));
    }

    uint32_t curr_poll_time = 0;
    uint32_t prev_poll_time = 0;
    while (g_enhanced_poll_ready)
    {
        uint32_t pad_bits;
        /* ensure at least 1ms has elapsed between polls
         * (beware of SD cab hardware compatibility)
         */
        curr_poll_time = timeGetTime();
        if (curr_poll_time == prev_poll_time)
        {
            curr_poll_time++;
            Sleep(1);
        }
        prev_poll_time = curr_poll_time;

        usbPadRead(&pad_bits);
        g_last_button_state = pad_bits;

        unsigned int buttonState = (g_last_button_state >> 8) & 0x1FF;
        for (int i = 0; i < 9; i++)
        {
            if ( ((buttonState >> i)&1) )
            {
                if (g_button_state[i] == -1)
                {
                    g_button_state[i] = curr_poll_time;
                }
                //debounce on release (since we're forced to stub usbPadReadLast)
                button_debounce[i] = g_debounce;
            }
            else
            {
                if (button_debounce[i]>0)
                {
                    button_debounce[i]--;
                }

                if (button_debounce[i] == 0)
                {
                    g_button_state[i] = -1;
                }
                else
                {
                    //debounce ongoing: flag button as still pressed
                    g_last_button_state |= 1<<(8+i);
                }
            }
        }
    }
    return 0;
}

uint32_t buttonGetMillis(uint8_t button)
{
    if (g_button_state[button] == -1)
        return 0;

    uint32_t but = g_button_state[button];
    uint32_t curr = timeGetTime();
    if (but <= curr)
    {
        uint32_t res = curr - but;
        return res;
    }

    return 0;
}

/* popnio.dll functions (for hardware offload) */
bool (__cdecl *openrealio)(void);
uint16_t (__cdecl *getbuttonstate)(uint8_t);
int (__cdecl *popnio_usbpadread)(uint32_t *);
int (__cdecl *popnio_usbpadreadlast)(uint8_t *);

uint32_t usbPadReadHook_addr = 0;
uint32_t usbPadReadLastHook_addr = 0;

int usbPadReadHook(uint32_t *pad_bits)
{
    // if we're here then ioboard is ready
    g_enhanced_poll_ready = true;

    // return last known input
    *pad_bits = g_last_button_state;
    return 0;
}

int usbPadReadHookHO(uint32_t *pad_bits)
{
    int res = popnio_usbpadread(pad_bits); // faster = more up-to-date

    // update last_button_state (for enhanced polling stats display)
    g_last_button_state = *pad_bits;
    return res;
}

uint16_t HO_get_button_timer(uint8_t index)
{
    uint16_t val = getbuttonstate(index);
    if ( val == 0xffff )
        return 0;
    return val;
}

uint32_t g_offset_fix[9] = {0};
uint8_t g_poll_index = 0;
void (*real_enhanced_poll)();
void patch_enhanced_poll() {
    /* eax contains button being checked [0-8]
     * esi contains delta about to be evaluated
     * we need to do esi -= buttonGetMillis([%eax]); to fix the offset accurately */
    __asm("push edx\n");

    __asm("cmp byte ptr [_g_hardware_offload], 0\n");
    __asm("je skip_ho\n");

    /* hardware offload */
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("push eax\n");
    __asm("call %P0" : : "i"(HO_get_button_timer));
    __asm("movzx ebx, ax\n");
    __asm("pop eax\n");
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("jmp button_state_empty\n");

    /* no hardware offload */
    __asm("skip_ho:\n");
    __asm("mov %0, al\n":"=m"(g_poll_index): :);

    /* reimplem buttonGetMillis(), result in ebx */
    __asm("xor ebx,ebx\n");
    __asm("mov edx,dword ptr ds:[eax*4+%0]\n"::"d"(g_button_state));
    __asm("cmp edx,0xFFFFFFFF\n");
    __asm("je button_state_empty\n");

    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call %P0" : : "i"(timeGetTime));
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("cmp edx,eax\n");

    __asm("jbe button_has_been_pressed\n");

    __asm("restore_eax:\n");
    __asm("movzx eax, %0\n": :"d"(g_poll_index));

    /* leave buttonGetMillis */
    __asm("button_state_empty:\n");
    __asm("sub esi, ebx\n"); // actual delta correction

    __asm("lea edx,dword ptr [eax*4+%0]\n"::"d"(g_offset_fix));
    __asm("mov [edx], ebx\n"::); // save correction value in g_offset_fix[g_poll_index];

    __asm("pop edx\n");
    __asm("mov eax, %0\n" : : "i"(&real_enhanced_poll));
    __asm("jmp [eax]");

    __asm("button_has_been_pressed:\n");
    __asm("sub eax,edx\n"); // eax = correction value (currTime - g_button_state[g_poll_index]);
    __asm("mov ebx, eax\n");// put correction value in ebx
    __asm("jmp restore_eax\n");
}

char enhancedusbio_str[] = "ENHANCED USB I/O";
static bool patch_usbio_string()
{
    uint32_t string_addr = (uint32_t)enhancedusbio_str;
    const char* as_hex = (const char*) &string_addr;
    /* change USB I/O to "ENHANCED USB I/O" */
    if (!find_and_patch_hex(g_game_dll_fn, "\x3F\x41\x56\x43\x41\x70\x70\x40\x40\x00\x00", 11, 0x0B, as_hex, 4))
    {
        LOG("WARNING: PopnIO: cannot replace USB I/O string in selftest menu\n");
        return false;
    }
    return true;
}

static bool patch_enhanced_polling_hardware_setup()
{
    bool res = false;

    HMODULE hinstLib = LoadLibrary("popnio.dll");

    if (hinstLib == NULL) {
        auto err = GetLastError();
        if ( err != 126 )
            LOG("ERROR: unable to load popnio.dll for hardware offload enhanced polling (error %ld)\n",err);

        return res;
    }

    LOG("popnhax: enhanced_polling: popnio.dll found, setup hardware offload\n");
    // Get functions pointers
    openrealio = (bool (__cdecl *)(void))GetProcAddress(hinstLib, "open_realio");
    getbuttonstate = (uint16_t (__cdecl *)(uint8_t))GetProcAddress(hinstLib, "get_button_state");
    popnio_usbpadread = (int (__cdecl *)(uint32_t *))GetProcAddress(hinstLib, "fast_usbPadRead");
    popnio_usbpadreadlast = (int (__cdecl *)(uint8_t *))GetProcAddress(hinstLib, "fast_usbPadReadLast");

    if ( openrealio == NULL || getbuttonstate == NULL || popnio_usbpadread == NULL || popnio_usbpadreadlast == NULL )
    {
        LOG("ERROR: a required function was not found in popnio.dll\n");
        goto cleanup;
    }

    if ( !openrealio() )
    {
        LOG("ERROR: PopnIO: device not found (make sure it is connected in REALIO mode and that the FX2LP driver is installed)\n");
        goto cleanup;
    }

    res = true;

cleanup:
    return res;
}

static HANDLE enhanced_polling_thread;

static bool patch_enhanced_polling(uint8_t debounce, bool stats)
{
    g_debounce = debounce;

    if ( !g_hardware_offload && enhanced_polling_thread == NULL) {
        if (stats)
        {
            enhanced_polling_thread = (HANDLE) _beginthreadex(
            NULL,
            0,
            enhanced_polling_stats_proc,
            NULL,
            0,
            NULL);
        }
        else
        {
            enhanced_polling_thread = (HANDLE) _beginthreadex(
            NULL,
            0,
            enhanced_polling_proc,
            NULL,
            0,
            NULL);
        }

    } // thread will remain dormant while g_enhanced_poll_ready == false

    /* patch eval timing function to fix offset depending on how long ago the button was pressed */
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize, "\xC6\x44\x24\x0C\x00\xE8", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: enhanced polling: cannot find eval timing function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x05;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)patch_enhanced_poll,
                      (void **)&real_enhanced_poll); // substract

    }

    /* patch calls to usbPadRead and usbPadReadLast */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x83\xC4\x04\x5D\xC3\xCC\xCC", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: enhanced polling: cannot find usbPadRead call (1)\n");
            return false;
        }
        pattern_offset = _search(data, dllSize-pattern_offset-1, "\x83\xC4\x04\x5D\xC3\xCC\xCC", 7, pattern_offset+1);
        if (pattern_offset == -1) {
            LOG("popnhax: enhanced polling: cannot find usbPadRead call (2)\n");
            return false;
        }

        if ( !g_hardware_offload )
        {
            usbPadReadHook_addr = (uint32_t)&usbPadReadHook;
            void *addr = (void *)&usbPadReadHook_addr;
            uint32_t as_int = (uint32_t)addr;

            // game will call usbPadReadHook instead of real usbPadRead (function call hook in order not to interfere with tools)
            uint64_t patch_addr = (int64_t)data + pattern_offset - 0x04; // usbPadRead function address
            patch_memory(patch_addr, (char*)&as_int, 4);

            // don't call usbPadReadLast as it messes with the 1000Hz polling, we'll be running our own debouncing instead
            patch_addr = (int64_t)data + pattern_offset - 20;
            patch_memory(patch_addr, (char*)"\x90\x90\x90\x90\x90\x90", 6);
        } else {
            // game will call popnio.dll fast_usbPadReadLast instead of the real usbPadReadLast
            usbPadReadLastHook_addr = (uint32_t)popnio_usbpadreadlast;
            void *addr = (void *)&usbPadReadLastHook_addr;
            uint32_t as_int = (uint32_t)addr;

            uint64_t patch_addr = (int64_t)data + pattern_offset - 18; // usbPadReadLast function address
            patch_memory(patch_addr, (char*)&as_int, 4);

            // game will call call popnio.dll fast_usbPadRead instead of the real usbPadRead
            usbPadReadHook_addr = (stats) ? (uint32_t)&usbPadReadHookHO : (uint32_t)popnio_usbpadread;
            addr = (void *)&usbPadReadHook_addr;
            as_int = (uint32_t)addr;

            patch_addr = (int64_t)data + pattern_offset - 0x04; // usbPadRead function address
            patch_memory(patch_addr, (char*)&as_int, 4);

        }

    }

    LOG("popnhax: enhanced polling enabled");
    if ( g_hardware_offload )
        LOG(" (with hardware offload)");
    else if (g_debounce != 0)
        LOG(" (%u ms debounce)", g_debounce);
    LOG("\n");

    return true;
}

void (*real_chart_load)();
void patch_chart_load_old() {
    /* This is mostly the same patch as the new version, except :
     * - eax and ecx have a different interpretation
     * - a chart chunk is only 2 dwords
     */
    __asm("cmp word ptr [edi+eax*8+4], 0x245\n");
    __asm("jne old_patch_chart_load_end\n");

    /* keysound event has been found, we need to convert timestamp */
    __asm("mov word ptr [edi+eax*8+4], 0x745\n"); // we'll undo it if we cannot apply it
    __asm("push eax\n"); // we'll convert to store chunk pointer (keysound timestamp at beginning)
    __asm("push ebx\n"); // we'll store a copy of our chunk pointer there
    __asm("push edx\n"); // we'll store the button info there
    __asm("push esi\n"); // we'll store chart growth for part2 subroutine there
    __asm("push ecx\n"); // we'll convert to store remaining chunks

    /* Convert eax,ecx registers to same format as later games so the rest of the patch is similar
     * TODO: factorize
     */
    __asm("sub ecx, eax\n"); // ecx is now remaining chunks rather than chart size
    __asm("imul eax, 8\n");
    __asm("add eax, edi\n"); //now eax is chunk pointer

    /* PART1: check button associated with keysound, then look for next note event for this button */
    __asm("xor dx, dx\n");
    __asm("mov dl, byte ptr [eax+7]\n");
    __asm("shr edx, 4\n"); //dl now contains button value ( 00-08 )

    __asm("mov ebx, eax\n"); //save chunk pointer into ebx for our rep movsd when a match is found
    __asm("old_next_chart_chunk:\n");
    __asm("add eax, 0x8\n"); // +0x08 in old chart format
    __asm("sub ecx, 1\n");
    __asm("jz old_end_of_chart\n");

    /* check where the keysound is used */
    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne old_check_first_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne old_check_first_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    //keysound change for this button before we found a key using it :(
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [edi+eax*8+4], 0x0\n"); // disable operation, cannot be converted to a 0x745, and should not apply
    __asm("jmp old_patch_chart_load_end\n");

    __asm("old_check_first_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne old_next_chart_chunk\n");
    /* found note event */
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne old_next_chart_chunk\n");
    /* found MATCHING note event */
    __asm("mov edx, dword ptr [ebx+4]\n"); //save operation (we need to shift the whole block first)

    /* move the whole block just before the note event to preserve timestamp ordering */
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov edi, ebx\n");
    __asm("mov esi, ebx\n");
    __asm("add esi, 0x08\n");
    __asm("mov ecx, eax\n");
    __asm("sub ecx, 0x08\n");
    __asm("sub ecx, ebx\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event just before our matching 0x145 which eax points to */
    __asm("mov dword ptr [eax-0x08+0x04], edx\n"); // operation
    __asm("mov edx, dword ptr [eax]\n");
    __asm("mov dword ptr [eax-0x08], edx\n"); // timestamp

    /* PART2: Look for other instances of same button key events (0x0145) before the keysound is detached */
    __asm("xor esi, esi\n"); //init chart growth counter

    /* look for next note event for same button */
    __asm("mov ebx, dword ptr [eax-0x08+0x04]\n"); //operation copy
    __asm("mov dl, byte ptr [eax+6]\n"); //dl now contains button value ( 00-08 )

    __asm("old_next_same_note_chunk:\n");
    __asm("add eax, 0x8\n");
    __asm("sub ecx, 1\n");
    __asm("jz old_end_of_same_note_search\n"); //end of chart reached

    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne old_check_if_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne old_check_if_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    __asm("jmp old_end_of_same_note_search\n"); // found matching 0x245 (keysound change for this button), we can stop search

    __asm("old_check_if_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne old_next_same_note_chunk\n");
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne old_next_same_note_chunk\n");

    //found a match! time to grow the chart..
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov esi, eax\n");
    __asm("mov edi, eax\n");
    __asm("add edi, 0x08\n");
    __asm("imul ecx, 0x08\n"); //ecx is number of chunks left, we want number of bytes for now, dword later
    __asm("std\n");
    __asm("add esi, ecx\n");
    __asm("sub esi, 0x04\n"); //must be on very last dword from chart
    __asm("add edi, ecx\n");
    __asm("sub edi, 0x04\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("cld\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event copy */
    // timestamp is already correct as it's leftover from the 0x145
    __asm("mov dword ptr [eax+0x04], ebx\n"); // operation
    __asm("add esi, 1\n"); //increase growth counter

    /* ebx still contains the 0x745 operation, dl still contains button, but eax points to 0x745 rather than the 0x145 so let's fix */
    /* note that remaining chunks is still correct due to growth, so no need to decrement ecx */
    __asm("add eax, 0x8\n");
    __asm("jmp old_next_same_note_chunk\n"); //look for next occurrence

    /* KEYSOUND PROCESS END */
    __asm("old_end_of_same_note_search:\n");
    /* restore before next timestamp */
    __asm("pop ecx\n");
    __asm("add ecx, esi\n"); // take chart growth into account
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");

    /* next iteration should revisit the same block since we shifted... anticipate the +0xC/-1/+0x64 that will be done by real_chart_load() */
    __asm("sub eax, 1\n");
    __asm("sub dword ptr [edi+eax*8], 0x64\n");
    __asm("jmp old_patch_chart_load_end\n");

    __asm("old_end_of_chart:\n");
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [edi+eax*8+4], 0x245\n"); // no match found (ad-lib keysound?), restore opcode
    __asm("old_patch_chart_load_end:\n");
    real_chart_load();
}

void patch_chart_load() {
    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne patch_chart_load_end\n");

    /* keysound event has been found, we need to convert timestamp */
    __asm("mov word ptr [eax+4], 0x745\n"); // we'll undo it if we cannot apply it
    __asm("push eax\n"); // chunk pointer (keysound timestamp at beginning)
    __asm("push ebx\n"); // we'll store a copy of our chunk pointer there
    __asm("push edx\n"); // we'll store the button info there
    __asm("push esi\n"); // we'll store chart growth for part2 subroutine there
    __asm("push ecx\n"); // remaining chunks

    /* PART1: check button associated with keysound, then look for next note event for this button */
    __asm("xor dx, dx\n");
    __asm("mov dl, byte ptr [eax+7]\n");
    __asm("shr edx, 4\n"); //dl now contains button value ( 00-08 )

    __asm("mov ebx, eax\n"); //save chunk pointer into ebx for our rep movsd when a match is found
    __asm("next_chart_chunk:\n");
    __asm("add eax, 0xC\n");
    __asm("sub ecx, 1\n");
    __asm("jz end_of_chart\n");

    /* check where the keysound is used */
    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne check_first_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne check_first_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    //keysound change for this button before we found a key using it :(
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [eax+4], 0x0\n"); // disable operation, cannot be converted to a 0x745, and should not apply
    __asm("jmp patch_chart_load_end\n");

    __asm("check_first_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne next_chart_chunk\n");
    /* found note event */
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne next_chart_chunk\n");
    /* found MATCHING note event */
    __asm("mov edx, dword ptr [ebx+4]\n"); //save operation (we need to shift the whole block first)

    /* move the whole block just before the note event to preserve timestamp ordering */
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov edi, ebx\n");
    __asm("mov esi, ebx\n");
    __asm("add esi, 0x0C\n");
    __asm("mov ecx, eax\n");
    __asm("sub ecx, 0x0C\n");
    __asm("sub ecx, ebx\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event just before our matching 0x145 which eax points to */
    __asm("mov dword ptr [eax-0x0C+0x04], edx\n"); // operation
    __asm("mov edx, dword ptr [eax]\n");
    __asm("mov dword ptr [eax-0x0C], edx\n"); // timestamp
    __asm("mov dword ptr [eax-0x0C+0x08], 0x0\n"); // cleanup possible longnote duration leftover

    /* PART2: Look for other instances of same button key events (0x0145) before the keysound is detached */
    __asm("xor esi, esi\n"); //init chart growth counter

    /* look for next note event for same button */
    __asm("mov ebx, dword ptr [eax-0x0C+0x04]\n"); //operation copy
    __asm("mov dl, byte ptr [eax+6]\n"); //dl now contains button value ( 00-08 )

    __asm("next_same_note_chunk:\n");
    __asm("add eax, 0xC\n");
    __asm("sub ecx, 1\n");
    __asm("jz end_of_same_note_search\n"); //end of chart reached

    __asm("cmp word ptr [eax+4], 0x245\n");
    __asm("jne check_if_note_event\n"); //still need to check 0x145..

    __asm("push ecx\n");
    __asm("xor cx, cx\n");
    __asm("mov cl, byte ptr [eax+7]\n");
    __asm("shr ecx, 4\n"); //cl now contains button value ( 00-08 )
    __asm("cmp cl, dl\n");
    __asm("pop ecx");
    __asm("jne check_if_note_event\n"); // 0x245 but not our button, still need to check 0x145..
    __asm("jmp end_of_same_note_search\n"); // found matching 0x245 (keysound change for this button), we can stop search

    __asm("check_if_note_event:\n");
    __asm("cmp word ptr [eax+4], 0x145\n");
    __asm("jne next_same_note_chunk\n");
    __asm("cmp dl, byte ptr [eax+6]\n");
    __asm("jne next_same_note_chunk\n");

    //found a match! time to grow the chart..
    __asm("push ecx\n");
    __asm("push esi\n");
    __asm("push edi\n");
    __asm("mov esi, eax\n");
    __asm("mov edi, eax\n");
    __asm("add edi, 0x0C\n");
    __asm("imul ecx, 0x0C\n"); //ecx is number of chunks left, we want number of bytes for now, dword later
    __asm("std\n");
    __asm("add esi, ecx\n");
    __asm("sub esi, 0x04\n"); //must be on very last dword from chart
    __asm("add edi, ecx\n");
    __asm("sub edi, 0x04\n");
    __asm("shr ecx, 2\n"); //div by 4 (sizeof dword)
    __asm("rep movsd\n");
    __asm("cld\n");
    __asm("pop edi\n");
    __asm("pop esi\n");
    __asm("pop ecx\n");

    /* write the 0x745 event copy */
    // timestamp is already correct as it's leftover from the 0x145
    __asm("mov dword ptr [eax+0x04], ebx\n"); // operation
    __asm("mov dword ptr [eax+0x08], 0x0\n"); // cleanup possible longnote duration leftover
    __asm("add esi, 1\n"); //increase growth counter

    /* ebx still contains the 0x745 operation, dl still contains button, but eax points to 0x745 rather than the 0x145 so let's fix */
    /* note that remaining chunks is still correct due to growth, so no need to decrement ecx */
    __asm("add eax, 0xC\n");
    __asm("jmp next_same_note_chunk\n"); //look for next occurrence

    /* KEYSOUND PROCESS END */
    __asm("end_of_same_note_search:\n");
    /* restore before next timestamp */
    __asm("pop ecx\n");
    __asm("add ecx, esi\n"); // take chart growth into account
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");

    /* next iteration should revisit the same block since we shifted... anticipate the +0xC/-1/+0x64 that will be done by real_chart_load() */
    __asm("sub eax, 0xC\n");
    __asm("add ecx, 1\n");
    __asm("sub dword ptr [eax], 0x64\n");
    __asm("jmp patch_chart_load_end\n");

    __asm("end_of_chart:\n");
    __asm("pop ecx\n");
    __asm("pop esi\n");
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop eax\n");
    __asm("mov word ptr [eax+4], 0x245\n"); // no match found (ad-lib keysound?), restore opcode
    __asm("patch_chart_load_end:\n");
    real_chart_load();
}

static bool patch_disable_keysound()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize, "\x00\x00\x72\x05\xB9\xFF", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: keysound disable: cannot find offset\n");
            return false;
        }

        /* detect if usaneko+ */
        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x0F;
        uint8_t check_byte = *((uint8_t *)(patch_addr + 1));

        if (check_byte == 0x04)
        {
            LOG("popnhax: keysound disable: old game version\n");
            //return false;
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)patch_chart_load_old,
                          (void **)&real_chart_load);
        }
        else
        {
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)patch_chart_load,
                          (void **)&real_chart_load); //rewrite chart to get rid of keysounds
        }

        LOG("popnhax: no more keysounds\n");
    }

    return true;
}

static bool patch_keysound_offset(int8_t value)
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    g_keysound_offset = -1*value;

    patch_add_to_base_offset(value);

    {
        int64_t pattern_offset = _search(data, dllSize, "\xC6\x44\x24\x0C\x00\xE8", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: keysound offset: cannot prepatch\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x07;
        patch_memory(patch_addr, (char *)"\x03", 1); // change "mov esi" into "add esi"

        _MH_CreateHook((LPVOID)(patch_addr-0x03), (LPVOID)patch_eval_timing,
                      (void **)&real_eval_timing); // preload esi with g_keysound_offset

        if (!config.audio_offset)
            LOG("popnhax: keysound offset: timing offset by %d ms\n", value);
        else
            LOG("popnhax: audio offset: audio offset by %d ms\n", -1*value);
    }

    return true;
}

static bool patch_add_to_beam_brightness(int8_t delta) {
    int32_t new_value = delta;
    char *as_hex = (char *) &new_value;

    LOG("popnhax: beam brightness: adding %d to beam brightness.\n",delta);

    uint32_t beam_brightness_addr;
    if (!get_addr_beam_brightness(&beam_brightness_addr))
    {
        LOG("popnhax: beam brightness: cannot find base address\n");
        return false;
    }

    int32_t current_value = *(int32_t *) beam_brightness_addr;
    new_value = current_value+delta;
    if (new_value < 0)
    {
        LOG("popnhax: beam brightness: fix invalid value (%d -> 0)\n",new_value);
        new_value = 0;
    }
    if (new_value > 255)
    {
        LOG("popnhax: beam brightness: fix invalid value (%d -> 255)\n",new_value);
        new_value = 255;
    }

    patch_memory(beam_brightness_addr, as_hex, 4);
    patch_memory(beam_brightness_addr+0x39, as_hex, 4);
    LOG("popnhax: beam brightness is now %d.\n",new_value);

    return true;
}

static bool patch_beam_brightness(uint8_t value) {
    uint32_t newvalue = value;
    char *as_hex = (char *) &newvalue;
    bool res = true;

    /* call get_addr_beam_brightness() so that it can still work after base value is overwritten */
    uint32_t beam_brightness_addr;
    get_addr_beam_brightness(&beam_brightness_addr);

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\x64\x00\x00\x00\xD9", 6, 0x3A, as_hex, 4))
    {
        LOG("popnhax: base offset: cannot patch HD beam brightness\n");
        res = false;
    }

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\x64\x00\x00\x00\xD9", 6, 1, as_hex, 4))
    {
        LOG("popnhax: base offset: cannot patch SD beam brightness\n");
        res = false;
    }

    return res;
}

uint16_t *g_course_id_ptr;
uint16_t *g_course_song_id_ptr;
void (*real_parse_ranking_info)();
void score_challenge_retrieve_addr()
{
    /* only set pointer if g_course_id_ptr is not set yet,
     * to avoid overwriting with past challenge data which
     * is sent afterwards
     */
    __asm("mov ebx, %0\n": :"b"(g_course_id_ptr));
    __asm("test ebx, ebx\n");
    __asm("jne parse_ranking_info\n");

    __asm("lea %0, [esi]\n":"=S"(g_course_id_ptr): :);
    __asm("lea %0, [esi+0x8D4]\n":"=S"(g_course_song_id_ptr): :);
    __asm("parse_ranking_info:\n");
    real_parse_ranking_info();
}

void (*score_challenge_prep_songdata)();
void (*score_challenge_song_inject)();
void (*score_challenge_retrieve_player_data)();
void (*score_challenge_is_logged_in)();
void (*score_challenge_test_if_normal_mode)();

void (*real_make_score_challenge_category)();
void make_score_challenge_category()
{
    __asm("push ecx\n");
    __asm("push ebx\n");
    __asm("push edi\n");

    if (g_course_id_ptr && *g_course_id_ptr != 0)
    {
        score_challenge_retrieve_player_data();
        score_challenge_is_logged_in();
        __asm("test al, al\n");
        __asm("je leave_score_challenge\n");

        score_challenge_test_if_normal_mode();
        __asm("test al, al\n");
        __asm("jne leave_score_challenge\n");

        __asm("mov cx, %0\n": :"r"(*g_course_song_id_ptr));
        __asm("mov dl,6\n");
        __asm("lea eax,[esp+0x8]\n");
        score_challenge_prep_songdata();
        __asm("lea edi,[esi+0x24]\n");
        __asm("mov ebx,eax\n");
        score_challenge_song_inject();
        __asm("mov byte ptr ds:[esi+0xA4],1\n");
    }

    __asm("leave_score_challenge:\n");
    __asm("pop edi\n");
    __asm("pop ebx\n");
    __asm("pop ecx\n");
}

/* all code handling score challenge is still in the game but the
 * function responsible for building and adding the score challenge
 * category to song selection has been stubbed. let's rewrite it
 */
static bool patch_score_challenge()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* Part1: retrieve course id and song id, useful and will simplify a little */
    {

        int64_t pattern_offset = _search(data, dllSize, "\x81\xC6\xCC\x08\x00\x00\xC7\x44\x24", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find course/song address\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)score_challenge_retrieve_addr,
                      (void **)&real_parse_ranking_info);
    }

    /* Part2: retrieve subfunctions which used to be called by the now stubbed function */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\x89\x08\x88\x50\x02", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find song data prep function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        score_challenge_prep_songdata = (void(*)())patch_addr;
    }
    {
        int64_t pattern_offset = _search(data, dllSize-0x60000, "\x8B\x4F\x0C\x83\xEC\x10\x56\x85\xC9\x75\x04\x33\xC0\xEB\x08\x8B\x47\x14\x2B\xC1\xC1\xF8\x02\x8B\x77\x10\x8B\xD6\x2B\xD1\xC1\xFA\x02\x3B\xD0\x73\x2B", 37, 0x60000);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find category song inject function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        score_challenge_song_inject = (void(*)())patch_addr;
    }
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\x01\x8B\x50\x14\xFF\xE2\xC3\xCC\xCC\xCC\xCC", 12, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find check if logged function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x24;

        score_challenge_retrieve_player_data = (void(*)())patch_addr;
    }
    {
        int64_t pattern_offset = _search(data, dllSize, "\xE8\xDB\xFF\xFF\xFF\x33\xC9\x84\xC0\x0F\x94", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find check if logged function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x20;

        score_challenge_is_logged_in = (void(*)())patch_addr;
    }
    {
        int64_t pattern_offset = _search(data, dllSize, "\xF7\xD8\x1B\xC0\x40\xC3\xE8", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: score challenge: cannot find check if normal mode function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x6;

        score_challenge_test_if_normal_mode = (void(*)())patch_addr;
    }

    /* Part3: "unstub" the score challenge category creation */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x83\xF8\x10\x77\x75\xFF\x24\x85", 8, 0);
        if (pattern_offset == -1) {
            pattern_offset = _search(data, dllSize, "\x83\xF8\x11\x77\x7C\xFF\x24\x85", 8, 0); // jam&fizz
            if (pattern_offset == -1) {
                LOG("popnhax: score challenge: cannot find category building loop\n");
                return false;
            }
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x66;

        uint32_t function_offset = *((uint32_t*)(patch_addr+0x01));
        uint64_t function_addr = patch_addr+5+function_offset;

        _MH_CreateHook((LPVOID)(function_addr), (LPVOID)make_score_challenge_category,
                      (void **)&real_make_score_challenge_category);
    }

    LOG("popnhax: score challenge reactivated (requires server support)\n");
    return true;
}

static bool patch_base_offset(int32_t value) {
    char *as_hex = (char *) &value;
    bool res = true;

    /* call get_addr_timing_offset() so that it can still work after timing value is overwritten */
    uint32_t original_timing;
    get_addr_timing_offset(&original_timing);

    uint32_t sd_timing_addr;
    get_addr_sd_timing(&sd_timing_addr);

    uint32_t hd_timing_addr;
    get_addr_hd_timing(&hd_timing_addr);

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\xC4\xFF\xFF\xFF", 5, 1, as_hex, 4))
    {
        LOG("popnhax: base offset: cannot patch base SD timing\n");
        res = false;
    }

    if (!find_and_patch_hex(g_game_dll_fn, "\xB8\xB4\xFF\xFF\xFF", 5, 1, as_hex, 4))
    {
        LOG("popnhax: base offset: cannot patch base HD timing\n");
        res = false;
    }

    return res;
}

static bool patch_hd_timing() {
    if (!patch_base_offset(-76))
    {
        LOG("popnhax: HD timing: cannot set HD offset\n");
        return false;
    }

    LOG("popnhax: HD timing forced\n");
    return true;
}

static bool patch_hd_resolution(uint8_t mode) {
    if (mode > 2)
    {
        LOG("ponhax: HD resolution invalid value %d\n",mode);
        return false;
    }

    /* set popkun and beam brightness to 85 instead of 100, like HD mode does */
    if (!patch_beam_brightness(85))
    {
        LOG("popnhax: HD resolution: cannot set beam brightness\n");
        return false;
    }

    /* set window to 1360*768 */
    if (!find_and_patch_hex(g_game_dll_fn, "\x0F\xB6\xC0\xF7\xD8\x1B\xC0\x25\xD0\x02", 10, -5, "\xB8\x50\x05\x00\x00\xC3\xCC\xCC\xCC", 9)
     && !find_and_patch_hex(g_game_dll_fn, "\x84\xc0\x74\x14\x0f\xb6\x05", 7, -5, "\xB8\x50\x05\x00\x00\xC3\xCC\xCC\xCC", 9))
    {
        LOG("popnhax: HD resolution: cannot find screen width function\n");
        return false;
    }

    if (!find_and_patch_hex(g_game_dll_fn, "\x0f\xb6\xc0\xf7\xd8\x1b\xc0\x25\x20\x01", 10, -5, "\xB8\x00\x03\x00\x00\xC3\xCC\xCC\xCC", 9))
        LOG("popnhax: HD resolution: cannot find screen height function\n");

    if (!find_and_patch_hex(g_game_dll_fn, "\x8B\x54\x24\x20\x53\x51\x52\xEB\x0C", 9, -6, "\x90\x90", 2))
        LOG("popnhax: HD resolution: cannot find screen aspect ratio function\n");


    if ( mode == 1 )
    {
        /* move texts (by forcing HD behavior) */
        if (!find_and_patch_hex(g_game_dll_fn, "\x1B\xC9\x83\xE1\x95\x81\xC1\x86", 8, -5, "\xB9\xFF\xFF\xFF\xFF\x90\x90", 7))
            LOG("popnhax: HD resolution: cannot move gamecode position\n");

        if (!find_and_patch_hex(g_game_dll_fn, "\x6a\x01\x6a\x00\x50\x8b\x06\x33\xff", 9, -7, "\xEB", 1))
            LOG("popnhax: HD resolution: cannot move credit/network position\n");
    }

    LOG("popnhax: HD resolution forced%s\n",(mode==2)?" (centered texts)":"");

    return true;
}

static bool patch_fps_uncap(uint16_t fps) {

    if (fps != 0)
    {
        /* TODO: fix in spicetools and remove this */
        uint8_t count = 0;
        while (find_and_patch_hex(NULL, "\x55\x31\xC0\x89\xE5\x57\x8B\x4D\x08\x8D\x79\x04\xC7\x01\x00\x00\x00\x00\x83\xE7\xFC\xC7\x41\x24\x00\x00\x00\x00\x29\xF9\x83\xC1\x28\xC1\xE9\x02\xF3\xAB\x8B\x7D\xFC\xC9\xC3", 43, 0, "\x31\xC0\xC3", 3))
        {
            count++;
        }

        if (count)
        {
            LOG("popnhax: frame_limiter: patched %u instance(s) of memset(a1, 0, 40) (bad usbPadReadLast io hook)\n", count);
        }

        uint8_t ft = (1000 + (fps / 2)) / fps; // rounded 1000/fps
        int8_t delta = 16-ft;
        int8_t newval = -1*delta-2;

        /* enforce fps rate */
        if (!find_and_patch_hex(g_game_dll_fn, "\x7E\x07\xB9\x0C\x00\x00\x00\xEB\x09\x85\xC9", 11, -1, "\xFF", 1))
        {
            LOG("popnhax: frame_limiter: cannot patch frame limiter\n");
            return false;
        }
        if (!find_and_patch_hex(g_game_dll_fn, "\x7E\x07\xB9\x0C\x00\x00\x00\xEB\x09\x85\xC9", 11, 2, "\x90\x90\x90\x90\x90", 5))
        {
            LOG("popnhax: frame_limiter: cannot patch frame limiter\n");
            return false;
        }

        /* adjust sleep time (original code is "add -2", replace with "add newval") */
        if (!find_and_patch_hex(g_game_dll_fn, "\x6A\x00\x83\xC1\xFE\x51\xFF", 7, 4, (char *)&newval, 1))
        {
            LOG("popnhax: frame_limiter: cannot patch frame limiter\n");
            return false;
        }

        LOG("popnhax: fps capped to %u fps (%ums frame time, new val %d)\n", fps, ft, newval);
        return true;
    }

    if (!find_and_patch_hex(g_game_dll_fn, "\x7E\x07\xB9\x0C\x00\x00\x00\xEB\x09\x85\xC9", 11, 0, "\xEB\x1C", 2))
    {
        LOG("popnhax: fps uncap: cannot find frame limiter\n");
        return false;
    }

    LOG("popnhax: fps uncapped\n");
    return true;
}

uint32_t g_autopin_offset = 0;
uint32_t g_pincode;
uint32_t g_last_enter_pincode;
void (*real_pincode)(void);
void hook_pincode()
{
    __asm("push eax\n");
    __asm("push ecx\n");
    __asm("push edx\n");
    __asm("call _timeGetTime@0\n");
    __asm("sub eax, [_g_last_enter_pincode]\n");
    __asm("cmp eax, 30000\n"); //30sec cooldown (will only try once)
    __asm("pop edx\n");
    __asm("pop ecx\n");
    __asm("ja enter_pincode\n"); //skip entering pin if cooldown value not reached
    __asm("pop eax\n");
    __asm("jmp call_real_pincode_handling\n");

    __asm("enter_pincode:\n");
    __asm("add [_g_last_enter_pincode], eax"); // place curr_time in g_last_enter_pincode (cancel sub eax, [_g_last_playsram])
    __asm("pop eax\n");

    __asm("push eax\n");
    __asm("mov eax, dword ptr [_g_pincode]\n");
    __asm("mov byte ptr [esi], 4\n");
    __asm("add esi, 0x30\n");
    __asm("mov [esi], eax\n");
    __asm("sub esi, 0x30\n");

    __asm("cmp dword ptr [_g_autopin_offset], 0\n");
    __asm("je skip_2_crash\n");
    __asm("mov eax, dword ptr [_g_autopin_offset]\n");
    __asm("add eax, edi\n");
    __asm("mov byte ptr [eax], 2\n");
    __asm("add eax, 4\n");
    __asm("mov byte ptr [eax], 4\n");

    __asm("skip_2_crash:\n");
    __asm("pop eax\n");
    __asm("call_real_pincode_handling:\n");
    real_pincode();
}

static bool patch_autopin()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    FILE *file = fopen("_pincode.secret", "r");
    char line[32];

    if(file == NULL)
    {
        LOG("popnhax: autopin: cannot open '_pincode.secret' file\n");
        return false;
    }

    if (!fgets(line, sizeof(line), file)){
        LOG("popnhax: autopin: cannot read '_pincode.secret' file\n");
        fclose(file);
        return false;
    }
    fclose(file);

    memcpy(&g_pincode, line, 4);

    /* retrieve offset where to put values to prevent a crash */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x1C\x10\xC7\x87", 4, 0);
        if (pattern_offset == -1) {
            LOG("WARNING: autopin: cannot find pincode special offset, might crash\n");
        }

        g_autopin_offset = *(uint32_t *)((int64_t)data + pattern_offset + 4);
    }

    /* auto enter pincode */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x33\xC4\x89\x44\x24\x14\xA1", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: autopin: cannot find pincode handling function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_pincode,
                      (void **)&real_pincode);
    }
    LOG("popnhax: autopin enabled\n");
    return true;
}

void (*real_song_options)();
void patch_song_options() {
    __asm("mov byte ptr[ecx+0xA15], 0\n");
    real_song_options();
}

void (*real_numpad0_options)();
void patch_numpad0_options() {
    __asm("mov byte ptr[ebp+0xA15], 1\n");
    real_numpad0_options();
}

/* ----------- r2nk226 ----------- */
/* popn23 or less Check */
static bool version_check() {
    bool old_db = false;
    int32_t shift = -9;
    int64_t pre_gchartaddr = 0;

    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* check Part 1: (21-23 , 24-27) */
    {
        int64_t pattern_offset = _search(data, dllSize,
            "\x70\x64\x61\x74\x61\x5F\x66\x69\x6C\x65\x6E\x61\x6D\x65", 14, 0); // "pdata_filename"
        if (pattern_offset == -1) {
            old_db = true;
            shift = -4;
            #if DEBUG == 1
                LOG("popnhax: 'pdata_filename' was not found. popn23 or less\n");
            #endif
        }
    }

    /* check Part 2: prepare for g_chartbase_addr */
    {
        pre_gchartaddr = _search(data, dllSize, "\x8A\xC8\xBA", 3, 0);
        if (pre_gchartaddr == -1) {
            #if DEBUG == 1
                LOG("popnhax: chart_baseaddr was not found\n");
            #endif
            return false;
        }
    }

    /* check Part 3: po21-po23 */
    if (old_db) {
        {
            uint32_t* check_E8 = (uint32_t *)((int64_t)data + pre_gchartaddr -5);
            uint8_t val = *check_E8;

            if (val == 0xE8) {
                LOG("popnhax: pop'n 21 sunny park\n");
                p_version = 1;
                shift = -9;
            }
        }

        {
            if (p_version == 0) {
                int64_t pattern_offset = _search(data, dllSize, "\x83\xF8\x03\x77\x6B", 5, 0);
                if (pattern_offset == -1) {
                    LOG("popnhax: pop'n 23 eclale\n");
                    p_version = 3;
                } else {
                    LOG("popnhax: pop'n 22 lapistoria\n");
                    p_version = 2;
                }
            }
        }
    } else {
        //old_db = false;
        int64_t pattern_offset = _search(data, dllSize, "\x03\xC7\x8D\x44\x01\x2A\x89\x10", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: pop'n 24 - pop'n 26 (not Unilabo) \n");
            p_version = 4;
        } else {
            LOG("popnhax: pop'n 27 Unilabo\n");
            p_version = 7;
        }
    }

    uint32_t *g_chartbase_addr;
    g_chartbase_addr = (uint32_t *)((int64_t)data + pre_gchartaddr + shift);
    chartbase_addr = *g_chartbase_addr;

    if (p_version == 0) {
        LOG("popnhax: Unknown pop'n version.\n");
        return false;
    }

    #if DEBUG == 1
        LOG("debug : chartbase_addr is 0x%X\n", chartbase_addr);
    #endif

    return true;
}

const char *popkun_change = "gmcmh.ifs";
void (*gm_ifs_load)();
void loadtexhook() {
    if(p_record) {
        __asm("cmp dword ptr [eax+0x0B], 0x2E6E6D63\n"); // gm 'cmn.' ifs
        __asm("jne change_skip\n");
        __asm("mov %0, eax\n"::"a"(popkun_change));
        __asm("change_skip:\n");
    }
    gm_ifs_load();
}

char *ifsname_ptr = NULL;
void (*get_ifs_name)();
void get_ifs_filename() {
    __asm("mov %0, eax\n":"=a"(ifsname_ptr): :);

    get_ifs_name();
}

uint32_t elapsed_time = 0;
void (*get_elapsed_time_hook)();
void get_elapsed_time() {
    __asm("mov %0, eax\n":"=a"(elapsed_time): :);

    get_elapsed_time_hook();
}

bool check_recdatafile(uint32_t check_notes) {
    uint32_t check_cool = 0;
    uint32_t check_great = 0;
    uint32_t check_good = 0;
    uint32_t check_bad = 0;
    uint32_t val = 0;
    for (uint32_t i=0; i < check_notes; i++) {
        if (recbinArray_loaded[i+2].judge == 2) {
            check_cool++;
        } else if ((recbinArray_loaded[i+2].judge == 3) || (recbinArray_loaded[i+2].judge == 4)) {
            check_great++;
        } else if ((recbinArray_loaded[i+2].judge == 5) || (recbinArray_loaded[i+2].judge == 6)) {
            check_good++;
        } else if ((recbinArray_loaded[i+2].judge == 1) || (recbinArray_loaded[i+2].judge == 7) || (recbinArray_loaded[i+2].judge == 8) || (recbinArray_loaded[i+2].judge == 9)) {
            check_bad++;
        }
    }
    uint32_t cg = (check_great << 16) | check_cool;
    uint32_t gb = (check_bad << 16) | check_good;
    val = recbinArray_loaded[0].judge | (recbinArray_loaded[0].button >> 8) | (recbinArray_loaded[0].flag >> 16) | (recbinArray_loaded[0].pad[0] >> 24);

    val = val ^ cg ^ gb ^ 0x672DE ^ recbinArray_loaded[0].timestamp;

    #if DEBUG == 1
        LOG("popnhax: recbin: cool(%d), great(%d), good(%d), bad(%d)\n",
                 check_cool, check_great, check_good, check_bad);
        LOG("popnhax: recbin:cg is %08X. gb is %08X. val is %08X.\n", cg, gb, val);

        uint32_t judge_1 = 0;
        uint32_t judge_7 = 0;
        uint32_t judge_8 = 0;
        uint32_t judge_9 = 0;
        uint32_t judge_B = 0;
        uint32_t judge_C = 0;
        for (uint32_t i=0; i < check_notes; i++) {
            if (recbinArray_loaded[i+2].judge == 1) {
                judge_1++;
            } else if (recbinArray_loaded[i+2].judge == 7) {
                judge_7++;
            } else if (recbinArray_loaded[i+2].judge == 8) {
                judge_8++;
            } else if (recbinArray_loaded[i+2].judge == 9) {
                judge_9++;
            } else if (recbinArray_loaded[i+2].judge == 0x0B) {
                judge_B++;
            } else if (recbinArray_loaded[i+2].judge == 0x0C) {
                judge_C++;
            }
        }
        LOG("popnhax: recbin: judge_1(%d), judge_7(%d), judge_8(%d), judge_9(%d), judge_B(%d), judge_C(%d)\n",
                 judge_1, judge_7, judge_8, judge_9, judge_B, judge_C);

    #endif

    uint16_t c_err = 0;
    if (recbinArray_loaded[0].recid != cg) {
        c_err |= 1;
    }
    if ((uint32_t)recbinArray_loaded[0].timing != gb) {
        c_err |= 2;
    }
    if (recbinArray_loaded[1].button != (uint8_t)~recbinArray_loaded[0].button) {
        c_err |= 4;
    }
    if (recbinArray_loaded[1].flag != (uint8_t)~recbinArray_loaded[0].flag) {
        c_err |= 8;
    }
    if (val == 0) {
        c_err |= 0x10;
    }
    if (c_err != 0) {
        LOG("popnhax: This file has been modified. (%X)\n", c_err);
        return false;
    } else {
        #if DEBUG == 1
            LOG("popnhax: recbin: check ok!\n");
        #endif
    }

    return true;
}

bool recording_memoryset() {
    uint32_t size = sizeof(struct REC) * 0xC80;
    if (recbinArray_writing == NULL) {
        recbinArray_writing = (struct REC*)malloc(size);
        if (recbinArray_writing == NULL) {
            return false;
        }

    #if DEBUG == 1
        LOG("popnhax: record: default size set.\n");
        LOG("popnhax: recbinArray_writing addr is 0x%p\n", recbinArray_writing);
    #endif

    }

    memset(recbinArray_writing, 0, size);
    recbinArray_writing[0].recid = chartbase_addr;
    return true;
}

void recid2str(char* mrecid) {
    const char *m_diff[8] = {"ep", "np", "hp", "op", "NN", "NH", "HN", "HH"};
    uint16_t m_idx = HIWORD(rec_musicid);
    uint16_t m_id = LOWORD(rec_musicid);
    uint8_t pattern = 0; // default (ep)
        switch (m_idx) {
            case 0x0101:
                pattern = 1;
                break;
            case 0x0202:
                pattern = 2;
                break;
            case 0x0303:
                pattern = 3;
                break;
            case 0x0404:
                pattern = 4;
                break;
            case 0x0405:
                pattern = 5;
                break;
            case 0x0504:
                pattern = 6;
                break;
            case 0x0505:
                pattern = 7;
                break;
        }
        snprintf(mrecid, 10, "%04d(%s)", m_id, m_diff[pattern]);
        LOG("popnhax: m_id(%04d), m_idx(%04X), mrecid(%s)\n", m_id, m_idx, mrecid);
}

static bool record_playdata_start() {
    const char *filename = NULL;

    SearchFile s;
    char recid[10];

    if (recbinArray_loaded == NULL && recbinArray_writing == NULL) {
        find_recdata = false;

        get_recPlayoptions();
        recid2str(recid);

        char currentDirectory[FILENAME_MAX];
        if (_getcwd(currentDirectory, sizeof(currentDirectory)) == NULL) {
            LOG("popnhax: currentDirectory not found.\n");
            return false;
        }

        char folderPath[FILENAME_MAX];
        snprintf(folderPath, sizeof(folderPath), "%s\\rec", currentDirectory);

        struct stat st;
        if (stat(folderPath, &st) != 0) {
            mkdir(folderPath);
            LOG("popnhax: make dir. (rec)\n");
        }

        s.search("rec", "bin", false);
        auto result = s.getResult();
    //    LOG("popnhax: record start : found %d bin files in rec\n", result.size());
        for (uint16_t i = 0; i < result.size(); i++) {
            filename = result[i].c_str() +4;
            //LOG("%d : %s\n", i, filename);
            if (strstr(result[i].c_str(), (char*)recid) != NULL) {
                LOG("popnhax: record: found matching recmusicid, end search\n");
                LOG("popnhax: record: filename = %s\n", filename);
                find_recdata = true;
                break;
            }
        }

        FILE *recbin;
        uint32_t size;
        uint32_t rec_elements;

        // recorddata.bin load -> recbinArray_loaded
        if (find_recdata) {
            char filePath[FILENAME_MAX];
            snprintf(filePath, sizeof(filePath), "%s\\rec\\%s", currentDirectory, filename);

            recbin = fopen(filePath, "rb");
            if (recbin == NULL) {
                LOG("popnhax: record: filePath is %s\n", filePath);
                LOG("popnhax: record: cannot open %s\n", filename);
                return false;
            }

            fseek(recbin, 0, SEEK_END);
            size = ftell(recbin);
            fseek(recbin, 0, SEEK_SET);

            rec_elements = size / sizeof(struct REC);

            recbinArray_loaded = (struct REC*)malloc(size);
            if (recbinArray_loaded == NULL) {
                LOG("popnhax: record: memory allocation failure.\n");
                return false;
            }

            memset(recbinArray_loaded, 0, size);
            fread(recbinArray_loaded, sizeof(struct REC), rec_elements, recbin);
            fclose(recbin);

            #if DEBUG == 1
                LOG("popnhax: recbinArray_loaded addr is 0x%p\n", recbinArray_loaded);
            #endif

            // check notes.
            uint32_t check_notes = LOWORD(recbinArray_loaded[0].timestamp);
            if (rec_elements -2 != check_notes) {
                LOG("popnhax: record: error in notes. recnotes(%02X)\n", rec_elements);
                find_recdata = false;
                stop_recchange = true;
            }
            if (check_recdatafile(check_notes)) {
                recbinArray_loaded[0].recid = chartbase_addr;
                recbinArray_loaded[0].timing = 0;
                //find_recdata = true;
                stop_recchange = false; // recmode_select enabled.
                __asm("mov eax, 0x20\n");
                __asm("push 0\n");
                __asm("call %0\n"::"D"(playsramsound_func));
                __asm("add esp, 4\n");
            } else {
                find_recdata = false;
                stop_recchange = true;
            }
        } else {
            // find_recdata = false
            LOG("popnhax: record: matching recmusicid not found.\n");
        }

        // next step: for recbinArray_writing
        if(!recording_memoryset()) {
            LOG("popnhax: record: memory allocation failure.\n");
        }
    }
    return true;
}

void (*real_musicselect)();
void hook_musicselect() {
    uint32_t ecxsafe;
    __asm("mov %0, ecx\n":"=c"(ecxsafe): :);
    // flag reset
    disp = false;
    find_recdata = false;
    p_record = false;
    recsavefile = false;
    is_resultscreen_flag = false;
    stop_input = false;
    stop_recchange = true;
//    speed = 5;
//    new_speed = 100;
//    regul_flag = false;
//    r_ran = false;
    use_sp_flag = false;
    **usbpad_addr = 1;

    if (recbinArray_loaded != NULL) {
        free(recbinArray_loaded);
        recbinArray_loaded = NULL;

        #if DEBUG == 1
            LOG("popnhax: load data memory free.\n");
        #endif
    }
    if (recbinArray_writing != NULL) {
        free(recbinArray_writing);
        recbinArray_writing = NULL;

        #if DEBUG == 1
            LOG("popnhax: recording data memory free.\n");
        #endif
    }
    __asm("mov ecx, %0\n"::"c"(ecxsafe));

    real_musicselect();
}

void rec_id_check() {
    __asm("push ecx\n");
    __asm("push ebp\n");
    __asm("mov eax, dword ptr [%0]\n"::"a"(p_note));
    __asm("mov esi, [eax]\n");
    __asm("mov eax, [%0]\n"::"a"(&recbinArray_loaded));
    __asm("mov ebp, eax\n");
    __asm("xor al, al\n");
    __asm("mov ecx, dword ptr [esi+0x04]\n");
    __asm("sub ecx, dword ptr [ebp+0x0C]\n");
    __asm("mov ebp, dword ptr [ebp+0x04]\n");
    __asm("cmp word ptr [ebp+0x0C], cx\n");
    __asm("jne rec_id_nomatch\n");

    // redID matched.
    __asm("movzx ecx, byte ptr [ebp+0x04]\n"); // judge
    __asm("cmp dword ptr [esp+0x08+0x04], 0x01\n"); // bad_check flag
    __asm("jne long_check\n");
    __asm("cmp ecx, 0x07\n"); // bad_check 7 - B
    __asm("jl long_check\n");
    __asm("xor ebx, ebx\n");
    __asm("or word ptr [esi+0x18], 0x02\n");
    __asm("cmp ecx, 0x09\n");
    __asm("jne long_check\n");
    __asm("mov ebx, 0x02\n");
    __asm("xor ecx, ecx\n");

    __asm("long_check:\n");
    __asm("mov eax, dword ptr [esi+0x04]\n");
    __asm("cmp dword ptr [eax+0x08], 0\n"); // long check
    __asm("jbe id_match_end\n");

    __asm("long_start:\n");
    __asm("cmp ecx, 0x07\n"); // bad_check 7 - B
    __asm("ja id_match_end\n");
    __asm("mov byte ptr [esi+0x60], cl\n");   // save long_judge

    __asm("id_match_end:\n");
    __asm("cmp cl, 0\n");
    __asm("setne al\n");

    __asm("rec_id_nomatch:\n");
    __asm("pop ebp\n");
    __asm("pop ecx\n");
}

void call_guidese() {
    uint8_t *g_guidese_addr;
    uint8_t stage_no = 0;
    uint32_t option_offset = 0;

    stage_no = *(uint8_t*)(**player_options_addr +0x0E);
    option_offset = player_option_offset * stage_no;
    g_guidese_addr = (uint8_t*)(**player_options_addr +0x37 +option_offset);
    __asm("movzx ecx, %0\n"::"a"(*g_guidese_addr));
    __asm("cmp cl, 0\n");
    __asm("je call_end\n");
    __asm("mov eax, dword ptr [esp+0x08]\n"); // judge+button
    __asm("and eax, 0x00FF\n"); // judge only
    __asm("cmp al, 0x02\n");
    __asm("jne se_continue\n");
    //COOL
    __asm("push 0\n");
    __asm("jmp se_call\n");
    __asm("se_continue:\n");
    __asm("cmp al, 0x04\n");
    __asm("je GOOD\n");
    __asm("cmp al, 0x03\n");
    __asm("je GOOD\n");
    __asm("cmp al, 0x06\n");
    __asm("je GREAT\n");
    __asm("cmp al, 0x05\n");
    __asm("je GREAT\n");
    __asm("jmp call_end\n");

    __asm("GREAT:\n");
    __asm("push 0x13880\n");
    __asm("jmp se_call\n");
    __asm("GOOD:\n");
    __asm("push 0x0EA60\n");
    __asm("se_call:\n");
    if (p_version == 7) {
        __asm("dec ecx\n");
    }
    __asm("mov eax, 0x21\n");
    __asm("add eax, ecx\n");
    __asm("call %0\n"::"D"(playsramsound_func));
    __asm("add esp, 4\n");
    __asm("call_end:\n");
}

void (*hook_playfirst)();
void play_firststep() {
    __asm("push eax\n");
    if (g_auto_flag == 0 && p_record) {
        __asm("push ebp\n");
        __asm("push ecx\n");
        __asm("push ebx\n");
        __asm("mov eax, [%0]\n"::"a"(&recbinArray_loaded));
        __asm("mov ebp, eax\n");
        __asm("movzx ecx, word ptr [ebp+0x08]\n");      // play_counter
        __asm("cmp edi, 0\n");                          // edi = elapsed time
        __asm("jne p1_start\n");
        __asm("mov word ptr [ebp+0x08], 0\n");         // play_counter reset

        __asm("p1_end:\n");
        __asm("xor eax, eax\n");
        __asm("jmp p1_endmerge\n");

        __asm("p1_start:\n");
        __asm("cmp word ptr [ebp], cx\n");              // [ebp] = rec_counter
        __asm("jle p1_end\n");
        __asm("add ecx, ecx\n");
        __asm("lea eax, dword ptr [ebp+ecx*8+0x20]\n");
        __asm("mov dword ptr [ebp+0x04], eax\n");

        __asm("debug_skip:\n");   // test_skip
        __asm("cmp word ptr [eax+0x06], 0x0101\n");
        __asm("je countup\n");

        __asm("cmp byte ptr [eax+0x06], 0xE2\n");
        __asm("jne skip_check\n");
        __asm("cmp edi, dword ptr [eax]\n");
        __asm("jl p1_end\n");                  // elapsed time < Timing -> return

        // E2はロング消えたあとのバグ？早GOODだけ入ることがあるのを再現…
        __asm("movzx ecx, word ptr [eax+0x04]\n");
        __asm("push edx\n");
        __asm("push ecx\n");
        call_guidese();
        __asm("add esp, 4\n");
        __asm("pop edx\n");
        __asm("mov eax, dword ptr [ebp+0x04]\n");

        __asm("countup:\n");
        __asm("inc word ptr [ebp+0x08]\n");
        __asm("add dword ptr [ebp+0x04], 0x10\n");
        __asm("jmp p1_end\n");

        __asm("skip_check:\n"); // judge=1 (poor)
        __asm("movzx ecx, byte ptr [eax+0x04]\n");
        __asm("cmp cl, 0x01\n");
        __asm("jle countup\n");     // 0.1 -> skip
        __asm("cmp cl, 0x0A\n");    // A.B~ -> skip_test
        __asm("jge countup\n");

        __asm("p1_idcheck:\n");
        __asm("push 0\n");      // id-checkOnly
        rec_id_check();
        __asm("add esp, 4\n");
        __asm("test al, al\n");
        __asm("je p1_end\n");
        // id matched
        __asm("mov eax, dword ptr [ebp+0x04]\n");

        __asm("input_continue:\n");
        __asm("mov eax, dword ptr [eax+0x08]\n");   // inputTiming
        __asm("neg eax\n");
        __asm("p1_endmerge:\n");
        __asm("pop ebx\n");
        __asm("pop ecx\n");
        __asm("pop ebp\n");
        __asm("add ecx, eax\n");
    }
    __asm("pop eax\n");
    hook_playfirst();
}

void (*last_auto_flag_check)();
void play_secondstep() {
    if (g_auto_flag == 1) {
        __asm("mov al, 0x01\n");
    } else if (g_auto_flag == 0) {
        if (p_record) {
            __asm("push edi\n");
            __asm("cmp ebx, 0x02\n");                   // long_end_flag
            __asm("jne p2_start\n");

            //__asm("p2_long_end_flow:\n");
            __asm("movzx eax, byte ptr [esi+0x12]\n");  // button
            __asm("movzx ecx, byte ptr [esi+0x60]\n");  // saved_judge
            __asm("shl ax, 8\n");
            __asm("or eax, ecx\n");
            __asm("push eax\n");                        // send judge
            __asm("call %0\n"::"a"(judge_bar_func));
            __asm("add esp, 4\n");
            __asm("mov ebx, 0x01\n");
            __asm("jmp p2_end\n");                      // no call guideSE

            __asm("p2_start:\n");
            __asm("mov edi, [%0]\n"::"D"(&recbinArray_loaded));
            __asm("mov eax, dword ptr [edi+0x04]\n");

            __asm("p2_continue:\n");
            __asm("movzx eax, word ptr [eax+0x04]\n");  // judge+button
            __asm("push eax\n");                        // send judge
            __asm("call %0\n"::"a"(judge_bar_func));
            __asm("pop eax\n");
            __asm("inc word ptr [edi+0x08]\n");         // play_count
            __asm("add dword ptr [edi+0x04], 0x10\n");
            __asm("push eax\n");                        // send judge
            call_guidese();
            __asm("add esp, 4\n");

            __asm("p2_end:\n");
            __asm("mov eax, [%0]\n"::"a"(p_note));
            __asm("mov esi, [eax]\n");
            __asm("mov ecx, esi\n"); // p_note
            __asm("pop edi\n");
            __asm("xor al, al\n");
        }
    }
    last_auto_flag_check();
}

void (*first_auto_flag_check)();
void play_thirdstep() {
    __asm("push edx\n");
    __asm("movzx eax, al\n");
    __asm("mov %0, eax\n":"=a"(g_auto_flag): :);
    if (g_auto_flag == 1) {
        __asm("mov al, 1\n"); // for DEMO play
    } else if (g_auto_flag == 0) {
        if (p_record) {
         // playback record
            __asm("mov edx, [%0]\n"::"d"(&recbinArray_loaded));
            __asm("mov ebx, [esi+0x04]\n");
            __asm("cmp dword ptr [ebx+0x08], 0\n"); // long check
            __asm("mov ebx, 0x01\n");
            __asm("jbe id_check\n");

            __asm("movzx eax, word ptr [esi+0x18]\n"); // flag
            __asm("test al, 0x80\n"); // long_end check
            __asm("jne p3_long_end\n");

            __asm("id_check:\n");
            __asm("push 1\n"); // recID & bad_check
            rec_id_check();
            __asm("add esp, 4\n");
            __asm("cmp ebx, 0x02\n");
            __asm("jne p3_return\n");

            __asm("inc word ptr [edx+0x08]\n");
            __asm("or word ptr [esi+0x18], 2\n");
            __asm("mov ebx, dword ptr [edx+0x04]\n");
            __asm("add dword ptr [edx+0x04], 0x10\n");
            __asm("movzx ebx, word ptr [ebx+0x04]\n");  // button&judge
            __asm("push ebx\n");
            __asm("call %0\n"::"a"(judge_bar_func));
            __asm("add esp, 4\n");
            __asm("xor al, al\n");
            __asm("mov ebx, 0x02\n");
            __asm("jmp p3_return\n");

            __asm("p3_long_end:\n");
            __asm("push 0\n");
            rec_id_check();
            __asm("add esp, 4\n");
            __asm("cmp al, bl\n");
            __asm("jne p3_end\n");
            __asm("inc word ptr [edx+0x08]\n"); // play_counter
            __asm("add dword ptr [edx+0x04], 0x10\n");

            __asm("p3_end:\n");
            __asm("mov eax, ebx\n");
        } else if (!p_record) {
            __asm("xor al, al\n"); // recording
        }
    }
    __asm("p3_return:\n");
    __asm("pop edx\n");
    first_auto_flag_check();
}

void (*long_end_flow)();
void play_fourthstep() {
    // movzx dx, byte ptr [esi+0x12]     // button
    // movzx ecx, byte ptr [esi+0x60]    // 0 (for long_bar clear)
    // shl dx, 8                         // 0xYY00 : YY = buttonNo.
    __asm("mov ebx, 0x02\n");            // long_end_flag. play_secondstep will catch it
    __asm("xor ecx, ecx\n");             // saved judge clear

    __asm("p4_continue:\n");
    if (g_auto_flag == 1) {
        __asm("dec ebx\n");
    }
    long_end_flow();
}

void (*get_judge)();
void record_playdata() {
    if (!p_record && stop_recchange) {
        // ax = ebp = judge+button
        if (elapsed_time > 0) {
            __asm("push ecx\n");
            __asm("mov ecx, ebp\n"); // judge+button
            __asm("push ebp\n");
            __asm("push ebx\n");

            __asm("mov eax, dword ptr [%0]\n"::"a"(&recbinArray_writing));
            __asm("mov ebp, eax\n");
            __asm("mov ebx, %0\n"::"b"(elapsed_time));

            __asm("rec1_start:\n");
            __asm("cmp cl, 0x0B\n");        // 空打ち
            __asm("jne rec1_calc_addr\n");  // 空じゃないならそのまま記録する
            __asm("cmp esi, 0\n");          // [esi]=p_note
            __asm("jmp rec1_end\n");        // 空打ちでp_noteが入ってる場合ロング空打ちだから飛ばす

            __asm("rec1_calc_addr:\n");
            __asm("mov eax, ecx\n");        // judge+button -> eax
            __asm("movzx ecx, word ptr [ebp]\n");  // rec_counter
            __asm("add ecx, ecx\n");
            __asm("lea ecx, dword ptr [ebp+ecx*8+0x20]\n");
            __asm("mov dword ptr [ebp+0x04], ecx\n");
            __asm("mov dword ptr [ecx], ebx\n");        // elapsed time
            __asm("mov dword ptr [ecx+0x04], eax\n");   // judge+button

            __asm("rec1_continue:\n");  // [ebx]==edi==buttonNo.
            __asm("mov eax, dword ptr [%0]\n"::"a"(j_win_addr));
            __asm("mov eax, dword ptr [eax+edi*8+4]\n");
            __asm("mov dword ptr [ecx+0x08], eax\n");     // input timing
            __asm("test esi, esi\n");   // [esi]=p_note
            __asm("jne rec1_recid\n"); //
            __asm("mov dword ptr [ecx+0x08], esi\n");
            __asm("mov dword ptr [ecx+0x0C], esi\n");
            __asm("jmp rec1_countup\n");

            __asm("rec1_recid:\n");
            __asm("mov eax, dword ptr [esi+0x04]\n");     // [p_note + 4] -> chart_addr
            __asm("sub eax, dword ptr [ebp+0x0C]\n");     // chart_addr - baseaddr -> recID
            __asm("mov dword ptr [ecx+0x0C], eax\n");     // recID

            __asm("rec1_testcheck:\n");
            __asm("mov eax, dword ptr [esi+0x18]\n");  // flags
            __asm("mov byte ptr [ecx+0x06], al\n");

            // long bad
            __asm("test al, 0x80\n");
            __asm("je rec1_countup\n");
            __asm("test al, 0x02\n");
            __asm("je rec1_countup\n");
            __asm("mov byte ptr [ecx+0x07], 0xFF\n");

            __asm("rec1_countup:\n");
            __asm("inc word ptr [ebp]\n");

            __asm("rec1_end:\n");
            __asm("pop ebx\n");
            __asm("pop ebp\n");
            __asm("pop ecx\n");
            __asm("mov eax, ebp\n"); // judge+button
        }
    }
    get_judge();
}

void (*get_poor)();
void record_playdata_poor() {
    if (!p_record && stop_recchange && !g_auto_flag) {
        // (g_auto_flag = true) -> demo screen
        __asm("push ecx\n");
        __asm("push ebx\n");
        __asm("push edx\n");
        __asm("push edi\n");
     //__asm("rec2_start:\n");
        __asm("mov edi, %0\n"::"D"(elapsed_time));
        __asm("mov ebx, dword ptr [%0]\n"::"b"(&recbinArray_writing));
        __asm("movzx ecx, word ptr [ebx]\n");           // rec_counter
        __asm("add ecx, ecx\n");
        __asm("lea eax, dword ptr [ebx+ecx*8+0x20]\n");
        __asm("mov dword ptr [ebx+0x04], eax\n");       // rec_addr
        __asm("inc word ptr [ebx]\n");                  // rec_counter
        __asm("mov word ptr [eax+0x04], dx\n");         // poor_judge+button
        __asm("mov word ptr [eax+0x06], 0x0101\n");     // skip_test flag
        __asm("mov dword ptr [eax], edi\n");            // elapsed time
        __asm("mov edx, dword ptr [esi+0x04]\n");       // p_note+4 -> chart_addr
        __asm("mov ecx, dword ptr [ebx+0x0C]\n");       // chartbase_addr
        __asm("sub edx, ecx\n");
        __asm("mov dword ptr [eax+0x0C], edx\n");       // recID
        __asm("pop edi\n");
        __asm("pop edx\n");
        __asm("pop ebx\n");
        __asm("pop ecx\n");
    }
    get_poor();
}

/*
int compare_sran(const void* a, const void* b) {
    return ((struct SRAN*)a)->chart_addr - ((struct SRAN*)b)->chart_addr;
}
*/
/*
uint16_t s_countmax;
bool srandom_set() {
    s_countmax = HIWORD(recbinArray_loaded[0].timing);
    uint32_t size = s_countmax * sizeof(struct SRAN);

    s_list = (struct SRAN*)malloc(size);
    if (s_list == NULL) {
        LOG("popnhax: s_list malloc error\n");
        return false;
    }
    memset(s_list, 0, size);

    uint32_t i,j = 0;
    uint16_t binsize = LOWORD(recbinArray_loaded[0].timestamp) +1;

    for (i = 1; i < binsize; i++) {
        if (recbinArray_loaded[i].flag == 1) {
            s_list[j].chart_addr = recbinArray_loaded[i].recid + chartbase_addr;
            s_list[j].button = recbinArray_loaded[i].button;
            j++;
        }
        //if (j > s_countmax) break;
    }
    qsort(s_list, s_countmax, sizeof(struct SRAN), compare_sran);

    LOG("popnhax: s_list_addr is 0x%p. s_countmax is %d\n", s_list, s_countmax);

    return true;
}
*/
/*
void (*real_noteque_addr)();
void noteque_rewrite() {
    if (p_record && s_list != NULL) {
        __asm("push edx\n");
        __asm("push ebx\n");
        __asm("push ecx\n");
        __asm("mov eax, %0\n": :"a"(&s_count));
        __asm("movzx ecx, word ptr [eax]\n");
        __asm("mov ebx, dword ptr [%0]\n"::"b"(&s_list));
        __asm("lea ebx, dword ptr [ebx+ecx*8]\n");
        __asm("cmp ebp, [ebx]\n");
        __asm("je s_continue\n");
            LOG("popnhax: s_list noteque_hook error!\n");
            p_record = false;
            __asm("jmp que_end\n");
        __asm("s_continue:\n");
            __asm("movzx ecx, byte ptr [ebx+0x04]\n");
            __asm("inc word ptr [eax]\n");
        __asm("que_end:\n");
            __asm("movzx eax, cl\n");
            __asm("pop ecx\n");
            __asm("pop ebx\n");
            __asm("pop edx\n");
    }
    __asm("movzx eax, cl\n");
    real_noteque_addr();
}
*/

int compare(const void* a, const void* b) {
    struct REC* elementA = (struct REC*)a;
    struct REC* elementB = (struct REC*)b;

    if (elementA->timestamp != elementB->timestamp) {
        return elementA->timestamp - elementB->timestamp;
    } else {
        return elementB->recid - elementA->recid;
    }
}

void load_recPlayoptions() {
    uint32_t temp = 0;
    uint32_t rec_cmp = 0;

    rec_reload = false;

    temp = recbinArray_loaded[0].timestamp +1;
    rec_cmp = (temp >> 24); // HIBYTE(temp)
    if (new_speed != rec_cmp) {
    //    LOG("popnhax: new_speed(%d) , rec_cmp(%d)\n", new_speed, rec_cmp);
        rec_reload = true;
        stop_recchange = true;
        g_return_to_options = true;
    }
    new_speed = rec_cmp;

    temp = temp >> 16;
    speed = temp & 0xF;
    rec_cmp = (temp & 0xF0) >> 4;
    if (regul_flag != (uint8_t)rec_cmp) {
    //    LOG("popnhax: regul_flag(%d) , rec_cmp(%d)\n", regul_flag, rec_cmp);
        rec_reload = true;
        stop_recchange = true;
        g_return_to_options = true;
    }
    regul_flag = rec_cmp;

    uint32_t *g_options_addr;
    uint8_t *g_rechispeed_addr;
    uint8_t stage_no = 0;
    uint32_t option_offset = 0;

    stage_no = *(uint8_t*)(**player_options_addr +0x0E);
    option_offset = player_option_offset * stage_no;

    g_options_addr = (uint32_t*)(**player_options_addr +0x34 +option_offset);
    *g_options_addr = (uint32_t)recbinArray_loaded[1].timestamp;

    rec_options = *g_options_addr;
    if (p_version == 4) {
        rec_options ^= 0x01000000; // guidese_flag reverse
    }
    g_rechispeed_addr = (uint8_t*)(**player_options_addr +0x2A +option_offset);
    *g_rechispeed_addr = recbinArray_loaded[1].judge;

}

bool recdata_save() {
    const char *filename = NULL;
    SearchFile s;

    char recid[10];
    char recdate[15];

    if (!p_record) {
        // (p_record) 0,record 1,playback
        save_recSPflags();

        uint32_t size = LOWORD(recbinArray_writing[0].timestamp);
        qsort(recbinArray_writing +2, size, sizeof(struct REC), compare);

        recid2str(recid);

        char currentDirectory[FILENAME_MAX];
        if (_getcwd(currentDirectory, sizeof(currentDirectory)) == NULL) {
            LOG("popnhax: currentDirectory not found.\n");
            return false;
        }

        char folderPath[FILENAME_MAX];
        snprintf(folderPath, sizeof(folderPath), "%s\\rec", currentDirectory);

        struct stat st;
        if (stat(folderPath, &st) != 0) {
            mkdir(folderPath);
            LOG("popnhax: make dir. (rec)\n");
        }

        __asm("mov esi, %0\n"::"S"(recdate));
        __asm("call %0\n"::"a"(date_func));

        filename = (char*)recid;

        char filePath[FILENAME_MAX];
        snprintf(filePath, sizeof(filePath), "%s\\rec\\%s-%s_%s.bin",
                             currentDirectory, filename, recdate, ifsname_ptr);
        FILE* output_file = fopen(filePath, "wb");
        if (output_file == NULL) {
            LOG("popnhax: output_file create error.\n");
            return false;
        }
        fwrite(recbinArray_writing, sizeof(struct REC), size +2 , output_file);
        fclose(output_file);
        LOG("popnhax: recording data save done.\n");
        LOG("popnhax: savefilePath is %s\n", filePath);

        #if DEBUG == 1
            LOG("popnhax: recid =%s\n", recid);
            LOG("popnhax: recdate =%s\n", recdate);
        #endif

        return true;
    }
    return false;
}

void prepare_for_play_record() {
    stop_recchange = true;
    get_recPlayoptions();

    if (p_record) {
        if (recbinArray_loaded == NULL) {
            LOG("popnhax: record: recbin load error. (2)\n");
            p_record = false;
        } else {
            r_ran = false;
            load_recPlayoptions();
            **usbpad_addr = 0;
        }
    /*    if ((uint8_t)rec_options == 3) {
            // S-Random の準備
            s_count = 0;
            if(!srandom_set()) {
                LOG("popnhax: error! S-random set.\n");
            }
        }
    } else
    */
    } else if (!p_record) {
        if(!recording_memoryset()) {
            LOG("popnhax: record: memory allocation failure. (2)\n");
        }
    }

}

void (*real_optionloop_after_pressing_red)();
void hook_optionloop_after_pressing_red() {
    __asm("push ebp\n");
    __asm("push ebx\n");
    __asm("push edx\n");
    prepare_for_play_record();
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop ebp\n");
    real_optionloop_after_pressing_red();
}

void (*real_optionloop_after_pressing_yellow)();
void hook_optionloop_after_pressing_yellow() {
    __asm("push ebp\n");
    __asm("push ebx\n");
    __asm("push edx\n");
    prepare_for_play_record();
    __asm("pop edx\n");
    __asm("pop ebx\n");
    __asm("pop ebp\n");
    real_optionloop_after_pressing_yellow();
}

/* R-RANDOM hook */
void (*real_get_random)();
void r_random() {
    uint8_t orig_plop = (uint8_t)rec_options; // & 0xFF;
    uint16_t *bt_0;
    uint16_t *v6;
    uint16_t v7;
    uint16_t v9;

    if (p_record) {
        // load lanes from rec.bin
        uint8_t *load_button = &recbinArray_loaded[1].pad[0];
        uint32_t bt = *(uint32_t *)button_addr;
        uint32_t i = 0;
        do
        {
            *(uint16_t *)(bt + 2*i) = *load_button;
            ++i;
            load_button++;
        } while (i < 9);

    } else if (!p_record && r_ran) {
        uint8_t *g_options_addr;
        uint8_t stage_no = 0;
        uint32_t option_offset = 0;

        stage_no = *(uint8_t*)(**player_options_addr +0x0E);
        option_offset = player_option_offset * stage_no;

        g_options_addr = (uint8_t*)(**player_options_addr +0x34 +option_offset);
        *g_options_addr = 1;

        __asm("push 0\n");
        __asm("mov ebx, %0\n"::"b"(*button_addr));
        __asm("call %0\n"::"a"(ran_func));
        __asm("add esp, 4\n");

        bt_0 = *(uint16_t **)button_addr;
        if (*bt_0 == 0) bt_0++;
        v9 = *bt_0; // start No.

        // 0:regular 1:mirror 2:random 3:S-random
        if (orig_plop == 0) {
            uint32_t bt = *(uint32_t *)button_addr;
            uint32_t i = 0;
            do
            {
                *(uint16_t *)(bt + 2*i) = i;
                ++i;
            } while (i < 9);
        } else if (orig_plop == 1) {
            uint32_t bt = *(uint32_t *)button_addr;
            uint32_t i = 0;
            do
            {
                *(uint16_t *)(bt + 2*i) = 8 - i;
                ++i;
            } while (i < 9);
        }
        // startNo + BaseNo -> R-ran
        uint32_t bt = *(uint32_t *)button_addr;
        uint32_t i = 0;
        do
        {
            v6 = (uint16_t *)(bt + 2*i);
            v7 = v9 + *v6;
            if (v7 >= 9) {
                v7 -= 9;
            }
            *v6 = v7;
            ++i;
        } while (i < 9);
    }
    real_get_random();
}

/* Get address for Special Menu */
uint32_t *g_rend_addr;
uint32_t *font_color;
uint32_t *font_rend_func;
static bool get_rendaddr()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize,
             "\x3b\xC3\x74\x13\xC7\x00\x02\x00\x00\x00\x89\x58\x04\x89\x58\x08", 16, 0);
        if (pattern_offset == -1) {
            return false;
        }
        g_rend_addr = (uint32_t *)((int64_t)data + pattern_offset -4);
        font_color = (uint32_t *)((int64_t)data + pattern_offset +0x24);
    }

    {
        int64_t pattern_offset = _search(data, dllSize,
             "\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x8B\x4C\x24\x0C\x8D\x44\x24\x10", 24, 0);
        if (pattern_offset == -1) {
            return false;
        }
        font_rend_func = (uint32_t *)((int64_t)data + pattern_offset +16);
    }

    #if DEBUG == 1
        LOG("popnhax: get_rendaddr: g_rend_addr is 0x%X\n", *g_rend_addr);
        LOG("popnhax: get_rendaddr: font_color is 0x%X\n", *font_color);
        LOG("popnhax: get_rendaddr: font_rend_func is 0x%p\n", font_rend_func);
    #endif

    return true;
}

/* 2dx speed change */
uint32_t *g_2dx_buffer = 0;
char *g_2dx_str = NULL;
const char *sdsystem = "/data/sd/system";
void wavheader_rewrite () {
    /* [edi] = xx.2dx
    0x00 - 0x17 : 2dx header /x32/x44/x58/x39...
    0x18 (4byte) : Chunk ID (RIFFheader) /x52/x49/x46/x46
    0x1C (4byte) : Chunk Data Size
    0x20 (4byte) : RIFF Type /x57/x41/x56/x45
    0x24 (4byte) : Chunk ID /x66/x6D/x74/x20
    0x28 (4byte) : Chunk Data Size
    0x2C (2byte) : Compression Code , 0x0002=MS ADPCM
    0x2E (2byte) : Number of Channels , 0x0002=stereo
    0x30 (4byte) : Sample Rate , 0x0000AC44=44100Hz
    0x34 (4byte) : byte/sec
    */
    disp = false;
    uint32_t temp = 0;
    double mul = 0;
    if(strncmp(g_2dx_str, sdsystem, 15) != 0) {
        if(strncmp(g_2dx_str, sdsystem, 9) == 0) {
            if (g_2dx_buffer == NULL) {
                LOG("popnhax: g_2dx_buffer is NULL\n");
                new_speed = 100;
                speed = 5;
                use_sp_flag = true;
                //goto speed_end;
            } else {
                mul = (double)(((double)new_speed)/100);
                temp = *(g_2dx_buffer + 0x0C);  //0x0C*4=0x30 SampleRate
                temp = (uint32_t)((double)temp*mul);
                *(g_2dx_buffer + 0x0C) = temp;
                // next step
                temp = *(g_2dx_buffer + 0x0D);  //0x0D*4=0x34 byte per sec.
                temp = (uint32_t)((double)temp*mul);
                *(g_2dx_buffer + 0x0D) = temp;

                use_sp_flag = true;
            }
        }
    }
//speed_end:
//    LOG("popnhax : 2dx speed change done.\n");
}

void (*real_2dx_addr)();
void ex_2dx_speed() {
    __asm("mov eax, ebp\n");
    __asm("mov %0, dword ptr [eax]\n":"=a"(g_2dx_str): :);
    __asm("mov %0, edi\n":"=D"(g_2dx_buffer): :);

    if(new_speed !=100) {
        wavheader_rewrite();
    }

    real_2dx_addr();
}

/* chart speed change */
uint32_t chart_rows_count = 0;
void chart_rewrite() {
    struct CHART {
        uint32_t timestamp;
        uint16_t operation;
        uint16_t data;
        uint32_t duration;
    } ;

    double mul = 0;
    double mul_2dx =0;
    uint32_t i, size;
    struct CHART* chart_temp;

    size = sizeof(struct CHART) * chart_rows_count;
    chart_temp = (struct CHART*)malloc(size);
    if (chart_temp == NULL || new_speed == 0) {
        LOG("popnhax: chart rewrite error(%d).\n", new_speed);
        new_speed = 100;
        speed = 5;
        regul_flag = false;
        goto chart_speed_end;
    }

    memcpy(chart_temp, (uint32_t*)chartbase_addr, size);

    mul = (double)(100/((double)new_speed));
    mul_2dx = (double)(((double)new_speed)/100);

    for (i = 0; i < chart_rows_count; i++) {
        chart_temp[i].timestamp = (uint32_t)((double)chart_temp[i].timestamp*mul);
        if (chart_temp[i].duration > 0) {
            chart_temp[i].duration = (uint32_t)((double)chart_temp[i].duration*mul);
        }
        if (chart_temp[i].operation == 0x0445) {
            uint16_t bpm_orig = 0;
            bpm_orig = chart_temp[i].data;
            if (regul_flag) {
                chart_temp[i].data = 0x64; // BPM set to 100
            } else {
                chart_temp[i].data = (uint16_t)((double)chart_temp[i].data*mul_2dx);
            }
            LOG("popnhax: BPM change %d -> %d \n", bpm_orig, chart_temp[i].data);
        }
    }

    memcpy((uint32_t*)chartbase_addr, chart_temp, size);

    #if DEBUG == 1
        LOG("popnhax: chartbase_addr is 0x%X\n", chartbase_addr);
        LOG("popnhax: chart_temp_addr is 0x%p\n", chart_temp);
    #endif

    free(chart_temp);

chart_speed_end:
    use_sp_flag = true;
    LOG("popnhax: chart speed change done.\n");

    #if DEBUG == 1
        LOG("popnhax: chart mul is %f\n", mul);
        LOG("popnhax: 2dx mul is %f\n", mul_2dx);
    #endif
}

void (*real_chart_addr)();
void ex_chart_speed() {
    __asm("mov %0, ebx\n":"=b"(chart_rows_count): :); // rows * 0x0C = chart_size
    disp = false;
    is_resultscreen_flag = false;
    stop_input = true; // Because the timing of loading charts and 2dx is different
    recsavefile = false;

    if (find_recdata) {
        // When find_recdata is true, record preparation is skipped.
        stop_recchange = false;
        recbinArray_loaded[0].timing = 0;
        if(!recording_memoryset()) {
            LOG("popnhax: record: memory allocation failure.\n");
        }
    } else {
        record_playdata_start();
    }

    if (new_speed !=100 || regul_flag) {
        chart_rewrite();
    }

    real_chart_addr();
}

#define COLOR_WHITE        0x00
#define COLOR_GREEN        0x10
#define COLOR_DARK_GREY    0x20
#define COLOR_YELLOW       0x30
#define COLOR_RED          0x40
#define COLOR_BLUE         0x50
#define COLOR_LIGHT_GREY   0x60
#define COLOR_LIGHT_PURPLE 0x70
#define COLOR_BLACK        0xA0
#define COLOR_ORANGE       0xC0

/* ----------- r2nk226 ----------- */
void flag_reset() {
    r_ran = false;
    regul_flag = false;
    is_resultscreen_flag = false;
    disp = false;
    use_sp_flag = false;
    stop_input = true;
    stop_recchange = true;
    p_record = false;
    find_recdata = false;
    rec_reload = false;
    speed = 5;
    new_speed = 100;

    if (recbinArray_loaded != NULL) {
        free(recbinArray_loaded);
        recbinArray_loaded = NULL;
        LOG("popnhax: reset: load data memory free.\n");
    }
    if (recbinArray_writing != NULL) {
        free(recbinArray_writing);
        recbinArray_writing = NULL;
        LOG("popnhax: reset: recording data memory free.\n");
    }

}

const char dmenu_1[] = "is_resultscreen_flag >> %d";
const char dmenu_2[] = "disp >> %d";
const char dmenu_3[] = "use_sp_flag >> %d";
const char dmenu_4[] = "stop_input >> %d";
const char dmenu_5[] = "stop_recchange >> %d";
const char dmenu_6[] = "p_record >> %d";
const char dmenu_7[] = "find_recdata >> %d";
const char dmenu_8[] = "speed >> %d";
const char dmenu_9[] = "rec_reload >> %d";
const char dmenu_10[] = "recsavefile >> %d";
const char dmenu_11[] = "rec_SPflags >> %02X";
const char dmenu_12[] = "rec_options >> %04X";
const char dmenu_13[] = "rec_hispeed >> %d";
void r2nk_debug() {
    __asm("push ebp\n");

    __asm("mov ebp, 0xD0\n");
    __asm("movzx ecx, %0\n"::"c"(spec));
    __asm("cmp ecx, 0x42\n");
    __asm("je shift_x\n");

    __asm("sub ebp, 0x40\n");
    __asm("shift_x:\n");

    __asm("call_dmenu9:\n");
    __asm("movzx eax, %0\n"::"a"(rec_reload));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_9));
    __asm("push 0x110\n");
    __asm("push ebp\n");
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu1:\n");
    __asm("movzx eax, %0\n"::"a"(is_resultscreen_flag));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_1));
    __asm("push 0x120\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu2:\n");
    __asm("movzx eax, %0\n"::"a"(disp));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_2));
    __asm("push 0x130\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu3:\n");
    __asm("movzx eax, %0\n"::"a"(use_sp_flag));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_3));
    __asm("push 0x140\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu4:\n");
    __asm("movzx eax, %0\n"::"a"(stop_input));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_4));
    __asm("push 0x150\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu5:\n");
    __asm("movzx eax, %0\n"::"a"(stop_recchange));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_5));
    __asm("push 0x160\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu6:\n");
    __asm("movzx eax, %0\n"::"a"(p_record));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_6));
    __asm("push 0x170\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu7:\n");
    __asm("movzx eax, %0\n"::"a"(find_recdata));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_7));
    __asm("push 0x180\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu8:\n");
    __asm("movzx eax, %0\n"::"a"(speed));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_8));
    __asm("push 0x190\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu10:\n");
    __asm("movzx eax, %0\n"::"a"(recsavefile));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_10));
    __asm("push 0x1A0\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu11:\n");
    __asm("movzx eax, %0\n"::"a"(rec_SPflags));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_11));
    __asm("push 0x1B0\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu12:\n");
    __asm("push %0\n"::"a"(rec_options));
    __asm("push %0\n"::"D"(dmenu_12));
    __asm("push 0x1C0\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("call_dmenu13:\n");
    __asm("movzx eax, %0\n"::"a"(rec_hispeed));
    __asm("push eax\n");
    __asm("push %0\n"::"D"(dmenu_13));
    __asm("push 0x1D0\n");
    __asm("push ebp\n");
    //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("pop ebp\n");
}
void enhanced_polling_stats_disp_sub();
const char menu_1[] = "--- Practice Mode ---";
const char menu_2[] = "Scores are not recorded."; //NO CONTEST
const char menu_3[] = "REGUL SPEED (pad4) >> %s";
const char menu_4[] = "R-RANDOM (pad6) >> %s";
//const char menu_5[] = "SPEED (pad5) >> %s";
const char *menu_6_str[2] = {"quick retire (pad9)", "quit pfree mode (pad9)"};
const char menu_7[] = "quick retry (pad8)";
const char *menu_9_str[2] = {"RECORD (pad00) >> %s", "SAVE FILE (pad00)%s"};
const char *menu_10_str[3] = {"No data.", "Record Available.", "OK"};
const char menu_12[] = "SPEED (pad5) >> %d%%";
const char *onoff_str[2] = {"OFF", "ON"};
const char *recplay_str[3] = {"rec", "play", " "};
void (*real_aging_loop)();
void new_menu() {
    __asm("mov eax, [%0]\n"::"a"(*g_rend_addr));
    __asm("cmp eax, 0\n");
    __asm("je call_menu\n");
    __asm("mov dword ptr [eax], 1\n");
    __asm("mov dword ptr [eax+4], 1\n");
    __asm("mov dword ptr [eax+8], 0\n");
    __asm("mov dword ptr [eax+0x34], 1\n");

    __asm("movzx ecx, %0\n"::"c"(spec));
    __asm("sub ecx, 0x42\n");
    __asm("sete cl\n");
    __asm("add byte ptr [eax+4], cl\n");

    __asm("lea ecx, dword ptr [%0+2]\n"::"a"(input_func));
    __asm("mov ecx, dword ptr [ecx]\n");
    __asm("mov ecx, dword ptr [ecx]\n");
    __asm("cmp ecx, 0\n");
    __asm("je menu_continue\n");
    __asm("cmp dword ptr [ecx+0x80], 5\n");
    __asm("je menu_continue\n");
    flag_reset();
    __asm("menu_continue:\n");
    if (!stop_recchange) {
        if (find_recdata) {
            __asm("mov ecx, 0x0B\n");
            __asm("call %0\n"::"a"(input_func));
            __asm("test al, al\n");
            __asm("je SW_00\n");
            p_record ^= 1;
            __asm("SW_00:\n");
        }
    } else if (stop_recchange && is_resultscreen_flag && !recsavefile) {
        __asm("mov ecx, 0x0B\n");
        __asm("call %0\n"::"a"(input_func));
        __asm("test al, al\n");
        __asm("je notsave\n");
            if(recdata_save()) {
                recsavefile = true;
            }
        __asm("notsave:\n");
    }

    if (!stop_input) {
        __asm("mov ecx, 4\n");
        __asm("call %0\n"::"a"(input_func));
        __asm("test al, al\n");
        __asm("je SW_4\n");
        regul_flag ^= 1;
        __asm("SW_4:\n");

        __asm("mov ecx, 6\n");
        __asm("call %0\n"::"a"(input_func));
        __asm("test al, al\n");
        __asm("je SW_6\n");
        r_ran ^= 1;
        __asm("SW_6:\n");

        __asm("mov ecx, 5\n");
        __asm("call %0\n"::"a"(input_func));
        __asm("test al, al\n");
        __asm("je SW_5\n");
        speed++;
        if (speed > 10) {
            speed = 0;
            new_speed = 150;
        }
        new_speed = 150 - (speed * 10);
        __asm("SW_5:\n");
    }

//Practice Mode--
    __asm("call_menu:\n");
    __asm("push %0\n"::"a"(menu_1));
    __asm("push 0x150\n");
    __asm("push 0x2E0\n");
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_BLUE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x0C\n");

//NO CONTEST
if (use_sp_flag) {
    __asm("push %0\n"::"a"(menu_2));
    __asm("push 0x160\n");
    __asm("push 0x2E0\n");
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_YELLOW));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x0C\n");
}

//Record playdata
    if (is_resultscreen_flag && !p_record) {
        __asm("push %0\n"::"a"(recplay_str[2]));
        __asm("push %0\n"::"a"(menu_9_str[1]));
        __asm("push 0x170\n");
        __asm("push 0x2E0\n");
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_YELLOW));
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x10\n");
    } else {
        __asm("push %0\n"::"a"(recplay_str[p_record]));
        __asm("push %0\n"::"a"(menu_9_str[0]));
        __asm("push 0x170\n");
        __asm("push 0x2E0\n");
        if (stop_recchange) {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_DARK_GREY));
        } else {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED)); // rec_mode
            if (p_record) {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_GREEN)); // play_mode
            }
        }
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x10\n");
    }

    if (recsavefile) {
        __asm("push %0\n"::"a"(menu_10_str[2]));
        __asm("push 0x180\n");
        __asm("push 0x2E0\n");
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x0C\n");
    } else {
        __asm("push %0\n"::"a"(menu_10_str[find_recdata]));
        __asm("push 0x180\n");
        __asm("push 0x2E0\n");
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_YELLOW));
        if (find_recdata) {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_GREEN));
        }
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x0C\n");
    }

//REGUL SPEED no-soflan
    __asm("push %0\n"::"D"(onoff_str[regul_flag]));
    __asm("push %0\n"::"a"(menu_3));
    __asm("push 0x190\n");
    __asm("push 0x2E0\n");
    if (stop_input) {
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_DARK_GREY));
    } else {
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_WHITE));
        if (regul_flag) {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
        }
    }
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

//R-RANDOM
    __asm("push %0\n"::"D"(onoff_str[r_ran]));
    __asm("push %0\n"::"a"(menu_4));
    __asm("push 0x1A0\n");
    __asm("push 0x2E0\n");
    if (stop_input) {
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_DARK_GREY));
    } else {
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_WHITE));
        if (r_ran) {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
        }
    }
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

//SPEED
    __asm("push %0\n"::"D"(new_speed));
    __asm("push %0\n"::"a"(menu_12));
    __asm("push 0x1B0\n");
    __asm("push 0x2E0\n");
    if (stop_input) {
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_DARK_GREY));
    } else {
        if (new_speed == 100) {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_WHITE));
        } else if (new_speed < 100) {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_YELLOW));
        } else {
            __asm("mov esi, %0\n"::"a"(*font_color+COLOR_RED));
        }
    }
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

/* quick menu on/off */
    if (disp) {

//quick retry
        __asm("push %0\n"::"a"(menu_7));
        __asm("push 0x1C0\n");
        __asm("push 0x2E0\n");
        __asm("mov esi, %0\n"::"a"(*font_color+COLOR_GREEN));
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x0C\n");

//quick retire & exit
        __asm("push %0\n"::"a"(menu_6_str[is_resultscreen_flag]));
        __asm("push 0x1D0\n");
        __asm("push 0x2E0\n");
        //__asm("mov esi, %0\n"::"a"(*font_color+COLOR_GREEN));
        __asm("call %0\n"::"a"(font_rend_func));
        __asm("add esp, 0x0C\n");

    }
    __asm("mov eax, [%0]\n"::"a"(*g_rend_addr));
    __asm("mov dword ptr [eax], 2\n");
    __asm("mov dword ptr [eax+4], 1\n");
    __asm("mov dword ptr [eax+8], 0\n");
    __asm("mov dword ptr [eax+0x34], 1\n");

#if DEBUG == 1
    r2nk_debug();
#endif

    if (config.enhanced_polling && config.enhanced_polling_stats)
        enhanced_polling_stats_disp_sub();

    real_aging_loop();
}

static bool patch_practice_mode()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        /* AGING MODE to Practice Mode */
        int64_t pattern_offset = _search(data, dllSize-0x100000,
                         "\x83\xEC\x40\x53\x56\x57", 6, 0x100000);

        if (pattern_offset == -1) {
            LOG("popnhax: cannot retrieve aging loop\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 6;

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)new_menu,
                      (void **)&real_aging_loop);

        #if DEBUG == 1
            LOG("popnhax: practice_mode: aging_hook addr is 0x%llX\n", patch_addr);
        #endif
    }
    {
        if (!get_rendaddr())
        {
            LOG("popnhax: Cannot find address for drawing\n");
            return false;
        }
    }
    {
        /* INPUT numkey */
        int64_t pattern_offset = _search(data, dllSize,
             "\x85\xC9\x74\x08\x8B\x01\x8B\x40\x24\x52\xFF\xD0", 12, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find input_func address\n");
            return false;
        }

        input_func = (uint32_t *)((int64_t)data + pattern_offset + 0x1A);

        #if DEBUG == 1
            LOG("popnhax: InputNumPad_addr is 0x%p\n", input_func);
        #endif
    }
    {
        /* player_options_addr */
        int64_t pattern_offset = _search(data, dllSize,
                 "\x14\xFF\xE2\xC3\xCC\xCC", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find player_options_addr\n");
            return false;
        }
        player_options_addr = (uint32_t **)((int64_t)data + pattern_offset + 0x31);
    }
    /* speed change */
    {
        // first step : 2dx hook
        int64_t pattern_offset = _search(data, dllSize,
                 "\x83\xC4\x0C\x8B\xC3\x8D\x7C\x24", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find 2dxLoad address\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset +0x10;

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)ex_2dx_speed,
                    (void **)&real_2dx_addr);

        #if DEBUG == 1
            LOG("popnhax: 2dxHook_addr is 0x%llX\n", patch_addr);
        #endif
    }
    {
        // second step : chart hook
        int64_t pattern_offset = _search(data, dllSize,
                 "\x8B\x74\x24\x18\x8D\x0C\x5B\x8B\x54\x8E\xF4", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find chartLoad address\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)ex_chart_speed,
                    (void **)&real_chart_addr);

        #if DEBUG == 1
            LOG("popnhax: chartHook_addr is 0x%llX\n", patch_addr);
        #endif
    }

    LOG("popnhax: speed hook enabled\n");

    /* r_random hook */
    {
        // random_function_addr
        int64_t pattern_offset = _search(data, dllSize,
             "\x51\x55\x56\xC7\x44\x24\x08\x00\x00\x00", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find random_function address\n");
            return false;
        }
        ran_func = (uint32_t *)((int64_t)data + pattern_offset);
    }
    {
        // button_addr
        int64_t pattern_offset = _search(data, dllSize,
                     "\x03\xC5\x83\xF8\x09\x7C\xDE", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find button address\n");
            return false;
        }
        button_addr = (uint32_t *)((int64_t)data + pattern_offset -0x14);
    }
    {
        // r-ran hook addr
        int64_t pattern_offset = _search(data, dllSize,
                 "\x83\xC4\x04\xB9\x02\x00\x00\x00", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find address for r-ran hook addr\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset -0x13;

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)r_random,
                    (void **)&real_get_random);
    }
    {
        // restore player options
        int64_t pattern_offset = _search(data, dllSize,
                     "\x5E\x8B\xE5\x5D\xC2\x04\x00\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x55\x8B\xEC\x83\xE4\xF8\x51\x56\x8B\xF1\x8B", 32, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: Cannot find address for restore addr\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset -11;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)restore_playoptions,
                    (void **)&restore_op);
    }

    #if DEBUG == 1
        LOG("popnhax: get_addr_random: ran_func_addr is 0x%p\n", ran_func);
        LOG("popnhax: get_addr_random: button_addr is 0x%X\n", *button_addr);
    #endif

    LOG("popnhax: R-Random hook enabled\n");

    return true;
}

/* ----------------------- */
/*    add a new feature    */
/* ----------------------- */
static bool patch_record_mode(bool quickretire)
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* If quickretire is not installed, it must be installed for reloading */
    if (!quickretire) {
        {
        /* hook quick retire transition to go back to option select instead */
            int64_t pattern_offset = _search(data, dllSize,
                     "\x8B\xE8\x8B\x47\x30\x83\xF8\x17", 8, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: quick retry: cannot retrieve screen transition function\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_screen_transition,
                        (void **)&real_screen_transition);
        }
        /* retrieve songstart function pointer for quick retry */
        {
            int64_t pattern_offset = _search(data, dllSize,
                     "\xE9\x0C\x01\x00\x00\x8B\x85", 7, 0);
            int delta = -4;

            if (pattern_offset == -1) {
                delta = 18;
                pattern_offset = _search(data, dllSize,
                     "\x6A\x00\xB8\x17\x00\x00\x00\xE8", 8, 0);
                if (pattern_offset == -1) {
                    LOG("popnhax: record reload: cannot retrieve song start function\n");
                    return false;
                }
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset + delta;
            g_startsong_addr = *(uint32_t*)(patch_addr);
        }
        /* instant launch song with numpad 8 on option select (hold 8 during song for quick retry) */
        {
            int64_t pattern_offset = _search(data, dllSize,
                     "\x8B\xF0\x83\x7E\x0C\x00\x0F\x84", 8, 0);

            if (pattern_offset == -1) {
                LOG("popnhax: quick retry: cannot retrieve option screen loop\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset - 0x0F;
            _MH_CreateHook((LPVOID)patch_addr, (LPVOID)quickexit_option_screen_simple,
                        (void **)&real_option_screen_simple);
        }

        LOG("popnhax: record reload: reloading enabled.\n");

    }
    /* record_mode hook */
    {
        //??_7CMusicSelectScene@@6B@
        int64_t pattern_offset = _search(data, dllSize,
                 "\x8B\x44\x24\x04\x56\x57\x50\x8B\xF9", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: MusicSelectScene_addr was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_musicselect,
                      (void **)&real_musicselect);
    }
    {
        // g_elapsed_time
        int64_t pattern_offset = _search(data, dllSize,
                 "\x02\x8B\xF0\x7C", 4, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: elapsed_time_addr was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset +1;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)get_elapsed_time,
                      (void **)&get_elapsed_time_hook);
    }
    /*
    {
        // NoteQue_func
        int64_t pattern_offset = _search(data, dllSize,
                 "\x0F\xB6\xC1\x88\x4F\x11\x88\x4F\x12", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: noteque_func_addr was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset +3;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)noteque_rewrite,
                      (void **)&real_noteque_addr);
    }
    */
    {
        // ifs_name
        int64_t pattern_offset = _search(data, dllSize,
                 "\x83\xC4\x04\x50\x8B\xC7\x85\xDB", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: ifs_name_ptr was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)get_ifs_filename,
                      (void **)&get_ifs_name);
    }
    {
        // for reload
        int64_t pattern_offset = _search(data, dllSize,
                 "\xE9\x0C\x01\x00\x00\x8B\x85", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: record reload: cannot retrieve song start function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset -0x14;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_optionloop_after_pressing_red,
                      (void **)&real_optionloop_after_pressing_red);
    }
    {
        // next step (after pressing yellow)
        int64_t pattern_offset = _search(data, dllSize,
             "\x8B\x55\x00\x8B\x82\x9C\x00\x00\x00\x6A\x01\x8B\xCD\xFF\xD0\x80\xBD", 17, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: record reload: cannot retrieve option screen yellow leave addr\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset +0x2C;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_optionloop_after_pressing_yellow,
                      (void **)&real_optionloop_after_pressing_yellow);
    }
    {
        // play1_addr(judge start)
        int64_t pattern_offset = _search(data, dllSize,
                 "\xC1\xE8\x07\x24\x01\x8A\xD8", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: recmode_forplay1_addr was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)play_firststep,
                      (void **)&hook_playfirst);
    }
    {
        // play3_addr (first_auto_flag_check)
        int64_t pattern_offset = _search(data, dllSize,
                 "\x84\xC0\x0F\x84\x08\x01\x00\x00", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: recmode_forplay3_addr was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)play_thirdstep,
                      (void **)&first_auto_flag_check);

        // play2_addr (last_auto_flag_check) , p_note
        int64_t pattern_offset_p2 = _search(data, dllSize,
                 "\x84\xC0\x74\x53", 4, pattern_offset);
        if (pattern_offset_p2 == -1) {
            LOG("popnhax: recmode_forplay2_addr was not found.\n");
            return false;
        }

        p_note = (uint32_t*)((int64_t)data + pattern_offset_p2 +6);
        uint64_t patch_addr_p2 = (int64_t)data + pattern_offset_p2;

        _MH_CreateHook((LPVOID)(patch_addr_p2), (LPVOID)play_secondstep,
                      (void **)&last_auto_flag_check);
    }
    {
        // play4_addr(long_end_flow)
        int64_t pattern_offset = _search(data, dllSize,
             "\x83\xC4\x04\xEB\x2E\xBA\x80\x00\x00\x00", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: recmode_forplay4_addr was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset -11;
        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)play_fourthstep,
                      (void **)&long_end_flow);
    }
    {
        // rec1_addr, judge_bar_func
        int64_t pattern_offset = _search(data, dllSize,
                 "\xE3\x00\x00\x83\xC4\x0C\x80\x7C\x24", 9, 0);
        if (pattern_offset == -1) {
            //next _search
            pattern_offset = _search(data, dllSize,
                 "\xE4\x00\x00\x83\xC4\x0C\x80\x7C\x24", 9, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: recmode_addr was not found.\n");
                return false;
            }
        }

        uint32_t *tmp_addr = (uint32_t*)((int64_t)data + pattern_offset -1);
        judge_bar_func = (uint32_t)((int64_t)data +pattern_offset + *tmp_addr +3);
        uint64_t patch_addr = (int64_t)data + pattern_offset -2;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)record_playdata,
                      (void **)&get_judge);
    }
    {
        // rec2_addr
        int64_t pattern_offset = _search(data, dllSize,
                 "\x24\x0F\x66\x0F\xB6\xC8\x66\xC1", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: recmode_addr2 was not found.\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset +0x10;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)record_playdata_poor,
                      (void **)&get_poor);
    }
    /* other functions */
    {
        // PlaySramSound func
        int64_t pattern_offset = _search(data, dllSize,
                 "\x51\x56\x8B\xF0\x85\xF6\x74\x6C\x6B\xC0\x2C", 11, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: PlaySramSound_addr was not found.\n");
            return false;
        }
        playsramsound_func = (uint32_t)((int64_t)data + pattern_offset);
    }
    {
        // j_win_addr
        int64_t pattern_offset = _search(data, dllSize,
                     "\x84\xC0\x74\x18\x8B\x04\xFD", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: j_win_addr was not found.\n");
            return false;
        }
        j_win_addr = (uint32_t)((int64_t)data + pattern_offset +7);
    }
    {
        // for rec_date
        int64_t pattern_offset = _search(data, dllSize, "\x83\xEC\x2C\x6A\x00", 5, 0);
        if (pattern_offset == -1) {
                LOG("popnhax: date_func was not found.\n");
                return false;
        }
        date_func = (uint32_t)((int64_t)data + pattern_offset);
    }
    {
        // for no-pfree
        int64_t pattern_offset = _search(data, dllSize,
             "\x83\xF8\x04\x0F\xB6\xC1\x75\x13\x69\xC0", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: record: player-option offset: not found.\n");
            return false;
        }
        uint32_t *temp_addr = (uint32_t *)((int64_t)data + pattern_offset +0x0A);
        player_option_offset = *temp_addr;
    }
    {
        // usbPadReadLast
        int64_t pattern_offset = _search(data, dllSize, "\x83\xC4\x04\x5D\xC3\xCC\xCC", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: record: cannot find usbPadRead call (1)\n");
            return false;
        }
        pattern_offset = _search(data, dllSize-pattern_offset-1, "\x83\xC4\x04\x5D\xC3\xCC\xCC", 7, pattern_offset+1);
        if (pattern_offset == -1) {
            LOG("popnhax: record: cannot find usbPadRead call (2)\n");
            return false;
        }
        usbpad_addr = (uint32_t**)((int64_t)data + pattern_offset -0x21);
    }
    {
        // recmode pop-kun change
        // Check if custom popkuns are present
        if (access("data_mods\\_popnhax_assets\\tex\\gmcmh.ifs", F_OK) == 0)
        {
            LOG("popnhax: record: custom popkun assets found. Using %s\n", popkun_change);

            int64_t pattern_offset = _search(data, dllSize,
                 "\x5E\x83\xC4\x10\xC3\x51\xE8", 7, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: record: ifs load address not found.\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset +13;

            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)loadtexhook,
                          (void **)&gm_ifs_load);

            #if DEBUG == 1
                LOG("popnhax: record: loadtexhook_addr is 0x%llX\n", patch_addr);
            #endif
        }
        else
        {
            LOG("popnhax: record: custom popkun assets not found. Using regular ones\n");
        }
    }

    #if DEBUG == 1
        LOG("popnhax: record: p_note is 0x%p, value is 0x%X\n", p_note, *p_note);
        LOG("popnhax: record: usbpad_addr is 0x%p, value is 0x%X\n", usbpad_addr, **usbpad_addr);
        LOG("popnhax: record: player_option_offset value is 0x%X\n", player_option_offset);
        LOG("popnhax: record: j_win_addr is 0x%X\n", j_win_addr);
        LOG("popnhax: record: judge_bar_func is 0x%X\n", judge_bar_func);
        LOG("popnhax: record: playsramsound_func is 0x%X\n", playsramsound_func);
        LOG("popnhax: record: date_func_addr is 0x%X\n", date_func);
        // LOG("popnhax: record: noteque_func_addr is 0x%X\n", noteque_func);
    #endif

    LOG("popnhax: record: rec_mode enabled.\n");

    return true;
}

void (*real_render_loop)();
void enhanced_polling_stats_disp()
{
    __asm("mov eax, [%0]\n"::"a"(*g_rend_addr));
    __asm("cmp eax, 0\n");
    __asm("je call_stat_menu\n");
    __asm("mov dword ptr [eax], 2\n");
    __asm("mov dword ptr [eax+4], 1\n");
    __asm("mov dword ptr [eax+8], 0\n");
    __asm("mov dword ptr [eax+0x34], 1\n");

    __asm("call_stat_menu:\n");
    enhanced_polling_stats_disp_sub();
    real_render_loop();
}

/* enhanced_polling_stats */
const char stats_disp_header[] = " - 1000Hz Polling - ";
const char stats_disp_lastpoll[] = "last input: 0x%06x";
const char stats_disp_avgpoll[] = "100 polls in %dms";
const char stats_disp_avgpoll_ho[] = "hardware offload";
const char stats_disp_offset_header[] = "- Latest correction -";
const char stats_disp_offset_top[] = "%s";
char stats_disp_offset_top_str[15] = "";
const char stats_disp_offset_bottom[] = "%s";
char stats_disp_offset_bottom_str[19] = "";
void enhanced_polling_stats_disp_sub()
{
    __asm("push %0\n"::"a"(stats_disp_header));
    __asm("push 0x2A\n"); //Y coord
    __asm("push 0x160\n"); //X coord
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_LIGHT_GREY));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x0C\n");

    uint8_t color = COLOR_GREEN;
    if (g_poll_rate_avg > 100)
        color = COLOR_RED;

    __asm("push %0\n"::"D"(g_poll_rate_avg));
    if ( g_hardware_offload )
    __asm("push %0\n"::"a"(stats_disp_avgpoll_ho));
    else
    __asm("push %0\n"::"a"(stats_disp_avgpoll));

    __asm("push 0x5C\n");
    __asm("push 0x160\n");
    __asm("mov esi, %0\n"::"a"(*font_color+color));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("push %0\n"::"D"(g_last_button_state));
    __asm("push %0\n"::"a"(stats_disp_lastpoll));
    __asm("push 0x48\n");
    __asm("push 0x160\n");
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_LIGHT_PURPLE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    __asm("push %0\n"::"a"(stats_disp_offset_header));
    __asm("push 0x2A\n");
    __asm("push 0x200\n");
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_LIGHT_GREY));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x0C\n");

    sprintf(stats_disp_offset_top_str, "%02u  %02u  %02u  %02u", g_offset_fix[1], g_offset_fix[3], g_offset_fix[5], g_offset_fix[7]);
    __asm("push %0\n"::"D"(stats_disp_offset_top_str));
    __asm("push %0\n"::"a"(stats_disp_offset_top));
    __asm("push 0x48\n");
    __asm("push 0x200\n");
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_ORANGE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");

    sprintf(stats_disp_offset_bottom_str, "%02u  %02u  %02u  %02u  %02u", g_offset_fix[0], g_offset_fix[2], g_offset_fix[4], g_offset_fix[6], g_offset_fix[8]);
    __asm("push %0\n"::"D"(stats_disp_offset_bottom_str));
    __asm("push %0\n"::"a"(stats_disp_offset_top));
    __asm("push 0x5C\n");
    __asm("push 0x1FD\n");
    __asm("mov esi, %0\n"::"a"(*font_color+COLOR_ORANGE));
    __asm("call %0\n"::"a"(font_rend_func));
    __asm("add esp, 0x10\n");
}

static bool patch_enhanced_polling_stats()
{
    if (config.practice_mode)
    {
        LOG("popnhax: enhanced polling stats displayed\n");
        return false;
    }
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize-0x100000, "\x83\xEC\x40\x53\x56\x57", 6, 0x100000);

        if (pattern_offset == -1) {
            LOG("popnhax: enhanced_polling_stats: cannot retrieve aging loop\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 6;
        if (!get_rendaddr())
        {
            LOG("popnhax: enhanced_polling_stats: cannot find address for drawing\n");
            return false;
        }

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)enhanced_polling_stats_disp,
                      (void **)&real_render_loop);

    }

    LOG("popnhax: enhanced polling stats displayed\n");
    return true;
}


/* HARD GAUGE SURVIVAL*/
uint8_t g_hard_gauge_selected = false;

void (*real_survival_flag_hard_gauge_new)();
void hook_survival_flag_hard_gauge_new()
{
    __asm("add eax, 0x36\n");
    __asm("cmp byte ptr [eax], 2\n");
    g_hard_gauge_selected = false;
    __asm("jne no_hard_gauge_new\n");
    g_hard_gauge_selected = true;
    __asm("no_hard_gauge_new:\n");
    __asm("sub eax, 0x36\n");
    real_survival_flag_hard_gauge_new();
}

void (*real_survival_flag_hard_gauge)();
void hook_survival_flag_hard_gauge()
{
    __asm("cmp bl, 0\n");
    __asm("jne no_hard_gauge\n");
    g_hard_gauge_selected = false;
    __asm("cmp cl, 2\n");
    __asm("jne no_hard_gauge\n");
    g_hard_gauge_selected = true;
    __asm("no_hard_gauge:\n");
    real_survival_flag_hard_gauge();
}

void (*real_survival_flag_hard_gauge_old)();
void hook_survival_flag_hard_gauge_old()
{
    __asm("cmp bl, 0\n");
    __asm("jne no_hard_gauge_old\n");
    g_hard_gauge_selected = false;
    __asm("cmp dl, 2\n"); //dl is used instead of cl in older games
    __asm("jne no_hard_gauge_old\n");
    g_hard_gauge_selected = true;
    __asm("no_hard_gauge_old:\n");
    real_survival_flag_hard_gauge_old();
}

void (*real_check_survival_gauge)();
void hook_check_survival_gauge()
{
    if ( g_hard_gauge_selected )
    {
        __asm("mov al, 1\n");
        __asm("ret\n");
    }
    real_check_survival_gauge();
}

void (*real_survival_gauge_medal_clear)();
void (*real_survival_gauge_medal)();
void hook_survival_gauge_medal()
{
    if ( g_hard_gauge_selected )
    {
        __asm("cmp eax, 0\n"); //empty gauge should still fail
        __asm("jz skip_force_clear\n");

        /* fix gauge ( [0;1023] -> [725;1023] ) */
        __asm("push eax");
        __asm("push ebx");
        __asm("push edx");
        __asm("xor edx,edx");
        __asm("mov eax, dword ptr [edi+4]");

            /* bigger interval for first bar */
        __asm("cmp eax, 42");
        __asm("jge skip_lower");
        __asm("mov eax, 42");
        __asm("skip_lower:");

            /* tweak off by one/two values */
        __asm("cmp eax, 297");
        __asm("je decrease_once");
        __asm("cmp eax, 298");
        __asm("je decrease_twice");
        __asm("cmp eax, 426");
        __asm("je decrease_once");
        __asm("cmp eax, 681");
        __asm("je decrease_once");
        __asm("cmp eax, 936");
        __asm("je decrease_once");
        __asm("cmp eax, 937");
        __asm("je decrease_twice");

        __asm("jmp no_decrease");

        __asm("decrease_twice:");
        __asm("dec eax");
        __asm("decrease_once:");
        __asm("dec eax");

        __asm("no_decrease:");

            /* perform ((gauge+99)/3) + 678 */
        __asm("add eax, 99");
        __asm("mov bx, 3");
        __asm("idiv bx");
        __asm("add eax, 678");

            /* higher cap value */
        __asm("cmp eax, 1023");
        __asm("jle skip_trim");
        __asm("mov eax, 1023");
        __asm("skip_trim:");

        __asm("mov dword ptr [edi+4], eax");
        __asm("pop edx");
        __asm("pop ebx");
        __asm("pop eax");

        __asm("jmp %0\n"::"m"(real_survival_gauge_medal_clear));
    }
    __asm("skip_force_clear:\n");
    real_survival_gauge_medal();
}

void (*real_get_retire_timer)();
void hook_get_retire_timer()
{
    if ( g_hard_gauge_selected )
    {
        __asm("mov eax, 0xFFFF\n");
        __asm("ret\n");
    }
    real_get_retire_timer();
}

bool patch_hard_gauge_survival(uint8_t severity)
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* refill gauge at each stage */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x84\xC0\x75\x0B\xB8\x00\x04\x00\x00", 9, 0, "\x90\x90\x90\x90", 4))
        {
            LOG("popnhax: survival gauge: cannot patch gauge refill\n");
        }
    }

    /* change is_survival_gauge function behavior */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x33\xC9\x83\xF8\x04\x0F\x94\xC1\x8A\xC1", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: survival gauge: cannot find survival gauge check function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x02;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_check_survival_gauge,
                      (void **)&real_check_survival_gauge);
    }

    /* change get_retire_timer function behavior (fix bug with song not exiting on empty gauge when paseli is on) */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x3D\xB0\x04\x00\x00\x7C", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: survival gauge: cannot find get retire timer function\n");
            return false;
        }

        int64_t fun_rel = *(int32_t *)(data + pattern_offset - 0x04 ); // function call is just before our pattern
        uint64_t patch_addr = (int64_t)data + pattern_offset + fun_rel;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_get_retire_timer,
                      (void **)&real_get_retire_timer);
    }

    /* hook commit option to flag hard gauge being selected */
    {
        /* find option commit function (unilab) */
        if (config.game_version == 27)
        {
            int64_t pattern_offset = _search(data, dllSize, "\x89\x48\x0C\x8B\x56\x10\x89\x50\x10\x66\x8B\x4E\x14\x66\x89\x48\x14\x5B\xC3\xCC", 20, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: survival gauge: cannot find option commit function (0)\n");
                return false;
            }
            uint64_t patch_addr = (int64_t)data + pattern_offset;
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_survival_flag_hard_gauge,
                        (void **)&real_survival_flag_hard_gauge);
        }
        else if (config.game_version < 27)
        {
            /* wasn't found, look for older function */
            int64_t first_loc = _search(data, dllSize, "\x0F\xB6\xC3\x03\xCF\x8D", 6, 0);

            if (first_loc == -1) {
                LOG("popnhax: survival gauge: cannot find option commit function (1)\n");
                return false;
            }

            int64_t pattern_offset = _search(data, 0x50, "\x89\x50\x0C", 3, first_loc);

            if (pattern_offset == -1) {
                LOG("popnhax: survival gauge: cannot find option commit function (2)\n");
                return false;
            }

            uint64_t patch_addr = (int64_t)data + pattern_offset;
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_survival_flag_hard_gauge_old,
                        (void **)&real_survival_flag_hard_gauge_old);
        }
        else
        {
            int64_t pattern_offset = _search(data, dllSize, "\x6B\xC9\x1A\x03\xC6\x8B\x74\x24\x10", 9, 0);
            if (pattern_offset == -1) {
                LOG("popnhax: survival gauge: cannot find option commit function (3)\n");
                return false;
            }
            uint64_t patch_addr = (int64_t)data + pattern_offset + 0x14;
            _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_survival_flag_hard_gauge_new,
                        (void **)&real_survival_flag_hard_gauge_new);
        }

    }

    /* Fix medal calculation */
    {
        int64_t addr = _search(data, dllSize, "\x0F\xB7\x47\x12\x66\x83\xF8\x14", 8, 0);
        if (addr == -1) {
            LOG("popnhax: survival gauge: cannot find medal computation\n");
            return false;
        }

        uint64_t function_addr = (int64_t)data + addr;
        real_survival_gauge_medal_clear = (void (*)())function_addr;

        int64_t pattern_offset = _search(data, dllSize, "\x0F\x9F\xC1\x5E\x8B\xD0\x3B\xC1\x7F\x02", 10, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: survival gauge: cannot find medal computation hook\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x04;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_survival_gauge_medal,
                      (void **)&real_survival_gauge_medal);
    }

    /* gauge severity */
    const char* severity_str[5] = { "", "COURSE (-35)", "NORMAL (-51)", "IIDX (-92)", "HARD (-204)"};
    uint32_t    severity_val[5] = {  0,  0xFFFFFFDD,     0xFFFFFFCD,     0xFFFFFFA4,   0xFFFFFF34};

    if (severity != 1)
    {
        if (severity > 4)
            severity = 4;
        uint32_t decrease_amount = severity_val[severity];
        char *as_hex = (char *) &decrease_amount;

        if (!find_and_patch_hex(g_game_dll_fn, "\xB8\xDD\xFF\xFF\xFF", 5, 1, as_hex, 4))
        {
            if (!find_and_patch_hex(g_game_dll_fn, "\xBA\xDD\xFF\xFF\xFF", 5, 1, as_hex, 4))
            {
                LOG("\n");
                LOG("popnhax: survival gauge: cannot patch severity\n");
            }
        }
    }

    if (!config.iidx_hard_gauge)
        LOG("popnhax: survival_gauge debug: enabled (decrease rate : %s)\n", severity_str[severity]);

    return true;
}

void (*real_survival_iidx_prepare_gauge)();
void hook_survival_iidx_prepare_gauge()
{
    //this code is specific to survival gauge, so no additional check is required
    __asm("cmp esi, 2\n");
    __asm("jne skip_iidx_prepare_gauge\n");
    __asm("shr eax, 1\n");

    __asm("skip_iidx_prepare_gauge:\n");
    real_survival_iidx_prepare_gauge();
}

void (*real_survival_iidx_apply_gauge)();
void hook_survival_iidx_apply_gauge()
{
    __asm("cmp byte ptr %0, 0\n"::"m"(g_hard_gauge_selected));
    __asm("je skip_iidx_apply_gauge\n"); //skip if not in survival gauge mode
    __asm("cmp ecx, 2\n");
    __asm("jb skip_iidx_apply_gauge\n"); //skip if gauge is not decreasing
    __asm("mov ecx, 3\n");
    __asm("cmp ax, 308\n");
    __asm("jge skip_iidx_apply_gauge\n"); //skip if gauge is above 30%
    __asm("mov ecx, 2\n");

    __asm("skip_iidx_apply_gauge:\n");
    real_survival_iidx_apply_gauge();
}

bool patch_survival_iidx()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    /* put half the decrease value in the first slot */
    {
        int64_t pattern_offset = _search(data, dllSize, "\xE9\x8C\x00\x00\x00\x8B\xC6", 7, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: iidx survival gauge: cannot find survival gauge prepare function\n");
            return false;
        }

        int32_t delta = 0;
        if (config.game_version > 27)
        {
            delta = 8;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - delta;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_survival_iidx_prepare_gauge,
                      (void **)&real_survival_iidx_prepare_gauge);
    }

    /* switch slot depending on gauge value (get halved value when 30% or less) */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x66\x83\xF8\x01\x75\x5E\x66\xA1", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: iidx survival gauge: cannot find survival gauge update function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x0C;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_survival_iidx_apply_gauge,
                      (void **)&real_survival_iidx_apply_gauge);
    }

    if (!config.iidx_hard_gauge)
        LOG("popnhax: survival_gauge debug: IIDX-like <=30%% adjustment\n");

    return true;
}

bool patch_survival_spicy()
{
    if ( config.game_version <= 27)
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xB9\x02\x00\x00\x00\x66\x89\x0C\x75", 9, 1, "\x00\x00\x00\x00", 4))
        {
            LOG("\n");
            LOG("popnhax: spicy survival gauge: cannot patch gauge increment\n");
            return false;
        }
    }
    else
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xBA\x02\x00\x00\x00\x66\x89\x14\x75", 9, 1, "\x00\x00\x00\x00", 4))
        {
            LOG("\n");
            LOG("popnhax: spicy survival gauge: cannot patch gauge increment\n");
            return false;
        }
    }

    if (!config.iidx_hard_gauge)
        LOG("popnhax: survival_gauge debug: spicy gauge\n");
    return true;
}

void (*skip_convergence_value_get_score)();
void (*real_convergence_value_compute)();
void hook_convergence_value_compute()
{
    __asm("push eax\n");
    __asm("mov eax, dword ptr [eax]\n"); // music id (edi-0x38 or edi-0x34 depending on game)
    __asm("cmp ax, 0xBB8\n"); // skip if music id is >= 3000 (cs_omni and user customs)
    __asm("jae force_convergence_value\n");
    __asm("pop eax\n");
    __asm("jmp %0\n"::"m"(real_convergence_value_compute));
    __asm("force_convergence_value:\n");
    __asm("pop eax\n");
    __asm("xor eax, eax\n");
    __asm("jmp %0\n"::"m"(skip_convergence_value_get_score));
}

void (*skip_pp_list_elem)();
void (*real_pp_increment_compute)();
void hook_pp_increment_compute()
{
    __asm("cmp ecx, 0xBB8\n"); // skip if music id is >= 3000 (cs_omni and user customs)
    __asm("jb process_pp_elem\n");
    __asm("jmp %0\n"::"m"(skip_pp_list_elem));
    __asm("process_pp_elem:\n");
    __asm("jmp %0\n"::"m"(real_pp_increment_compute));
}

static bool patch_db_fix_cursor(){
    /* bypass song id sanitizer */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x0F\xB7\x06\x66\x85\xC0\x7C\x1C", 8, -5, "\x90\x90\x90\x90\x90", 5))
        {
            LOG("popnhax: patch_db: cannot fix cursor\n");
            return false;
        }
    }
    /* skip 2nd check */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x0F\xB7\x06\x66\x85\xC0\x7C\x1C", 8, 0x1A, "\xEB", 1))
        {
            LOG("popnhax: patch_db: cannot fix cursor (2)\n");
            return false;
        }
    }
    return true;
}

void (*real_pp_mean_compute)();
void hook_pp_mean_compute()
{
    __asm("test ecx, ecx\n");
    __asm("jnz divide_list\n");
    __asm("mov eax, 0\n");
    __asm("jmp skip_divide\n");
    __asm("divide_list:\n");
    __asm("div ecx\n");
    __asm("skip_divide:\n");
    real_pp_mean_compute();
}

void (*real_pp_convergence_loop)();
void hook_pp_convergence_loop()
{
    __asm("movzx eax, word ptr[ebx]\n");
    __asm("cmp eax, 0xBB8\n");
    __asm("jl conv_loop_rearm\n");
    __asm("mov al, 0\n");
    __asm("jmp conv_loop_leave\n");
    __asm("conv_loop_rearm:\n");
    __asm("mov al, 1\n");
    __asm("conv_loop_leave:\n");
    real_pp_convergence_loop();
}

bool patch_db_power_points()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    int64_t child_fun_loc = 0;
    {
        int64_t offset = _search(data, dllSize, "\x8D\x46\xFF\x83\xF8\x0A\x0F", 7, 0);
        if (offset == -1) {
        #if DEBUG == 1
            LOG("popnhax: patch_db: failed to retrieve struct size and offset\n");
        #endif
            return false;
        }
        uint32_t child_fun_rel = *(uint32_t *) ((int64_t)data + offset - 0x04);
        child_fun_loc = offset + child_fun_rel;
    }

    {
        int64_t pattern_offset = _search(data, 0x40, "\x8d\x74\x01", 3, child_fun_loc);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: failed to retrieve offset from base\n");
            g_pfree_song_offset = 0x54; // best effort
            return false;
        }

        g_pfree_song_offset = *(uint8_t *) ((int64_t)data + pattern_offset + 0x03);
    }

    /* skip cs_omni and customs in power point convergence value */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8B\x6C\x24\x30\x8B\x4C\x24\x2C", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: cannot find power point convergence value computation loop\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x08;
        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_pp_convergence_loop,
                      (void **)&real_pp_convergence_loop);
    }

    /* make sure they cannot count (sanity check) */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x84\xC0\x75\x11\x8D\x44\x24\x38", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: cannot find convergence value computation\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x08;

        skip_convergence_value_get_score = (void(*)()) (patch_addr + 0x05);
        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_convergence_value_compute,
                      (void **)&real_convergence_value_compute);
    }

    /* skip cs_omni and customs in new stages pplist */
    {
        int64_t pattern_offset = _search(data, dllSize, "\x8A\x1E\x6A\x00\x51\xE8", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: cannot find pp increment computation\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x02;

        _MH_CreateHook((LPVOID)(patch_addr), (LPVOID)hook_pp_increment_compute,
                      (void **)&real_pp_increment_compute);

        int64_t jump_addr_offset = _search(data, dllSize, "\x8B\x54\x24\x5C\x0F\xB6\x42\x0E\x45", 9, 0);
        if (jump_addr_offset == -1) {
            LOG("popnhax: patch_db: cannot find pp increment computation next iter\n");
            return false;
        }
        skip_pp_list_elem = (void(*)()) ((int64_t)data + jump_addr_offset);
    }

    /* prevent crash when playing only customs in a credit */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x0F\x8E\x5C\xFF\xFF\xFF\xEB\x04", 8, 6, "\x90\x90", 2))
        {
            LOG("popnhax: patch_db: cannot patch end list pointer\n");
        }
    }

    /* prevent another crash when playing only customs in a credit (sanity check) */
    {
        int64_t pattern_offset = _search(data, dllSize, "\xC1\xF9\x02\x33\xD2\xF7\xF1\x8B\xC8", 9, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: patch_db: cannot find power point mean computation\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x05;
        patch_memory(patch_addr, (char*)"\x90\x90", 2); // erase original div ecx (is taken care of in hook_pp_mean_compute)

        /* fix possible divide by zero error */
        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_pp_mean_compute,
                     (void **)&real_pp_mean_compute);
    }

    LOG("popnhax: patch_db: power point computation fixed\n");
    return true;
}

static bool option_full()
{
    /* patch default values in memory init function */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\x88\x48\x1A\x88\x48\x1B\x88\x48\x1C", 9, 0, "\xC7\x40\x1A\x00\x00\x01\x00\x90\x90", 9))
        {
            LOG("popnhax: cannot set full options by default\n");
            return false;
        }
    }

    LOG("popnhax: always display full options\n");
    return true;
}

static bool option_guide_se_off(){
    /* set guide SE OFF by default in all modes */
    {
        if ( config.game_version < 27 )
        {
            if (!find_and_patch_hex(g_game_dll_fn, "\x89\x48\x20\x88\x48\x24\xC3\xCC", 8, 3, "\xC6\x40\x24\x01\xC3", 5))
            {
                LOG("popnhax: guidese_off: cannot set guide SE off by default\n");
                return false;
            }
        }
        else if ( config.game_version == 27 )
        {
            if (!find_and_patch_hex(g_game_dll_fn, "\xC6\x40\x24\x01\x88\x48\x25", 7, 3, "\x00", 1))
            {
                LOG("popnhax: guidese_off: cannot set guide SE off by default\n");
                return false;
            }
        }
        else
        {
            if (!find_and_patch_hex(g_game_dll_fn, "\xC6\x40\x24\x01\xC6\x40\x25\x03", 8, 3, "\x00", 1))
            {
                LOG("popnhax: guidese_off: cannot set guide SE off by default\n");
                return false;
            }
        }
    }
    LOG("popnhax: guidese_off: Guide SE OFF by default\n");
    return true;
}

static bool option_net_ojama_off(){
    /* set netvs ojama OFF by default */
    {
        if (!find_and_patch_hex(g_game_dll_fn, "\xC6\x40\xFD\x00\xC6\x00\x01", 7, 6, "\x00", 1))
        {
            LOG("popnhax: netvs_off: cannot set net ojama off by default\n");
            return false;
        }
    }
    LOG("popnhax: netvs_off: net ojama OFF by default\n");
    return true;
}

uint8_t get_version()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);
    {
        int64_t pattern_offset = search(data, dllSize, "\x00\x8B\x56\x04\x0F\xB7\x02\xE8", 8, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: get_version: cannot retrieve game version (eclale or less?)\n");
            return 0;
        }

        uint8_t version = *(uint8_t*)(data + pattern_offset + 14);
        return version;
    }
}

static bool get_music_limit_from_file(const char *filepath, uint32_t *limit){
    HANDLE hFile;
    HANDLE hMap;
    LPVOID lpBasePtr;
    LARGE_INTEGER liFileSize;

    hFile = CreateFile(filepath,
        GENERIC_READ,                          // dwDesiredAccess
        0,                                     // dwShareMode
        NULL,                                  // lpSecurityAttributes
        OPEN_EXISTING,                         // dwCreationDisposition
        FILE_ATTRIBUTE_NORMAL,                 // dwFlagsAndAttributes
        0);                                    // hTemplateFile

    if (hFile == INVALID_HANDLE_VALUE) {
        //file not existing is actually a good thing
        return false;
    }

    if (!GetFileSizeEx(hFile, &liFileSize)) {
        LOG("popnhax: auto_diag: GetFileSize failed with error %ld\n", GetLastError());
        CloseHandle(hFile);
        return false;
    }

    if (liFileSize.QuadPart == 0) {
        LOG("popnhax: auto_diag: popn22.dll file is empty?!\n");
        CloseHandle(hFile);
        return false;
    }

    hMap = CreateFileMapping(
        hFile,
        NULL,                          // Mapping attributes
        PAGE_READONLY,                 // Protection flags
        0,                             // MaximumSizeHigh
        0,                             // MaximumSizeLow
        NULL);                         // Name
    if (hMap == 0) {
        LOG("popnhax: auto_diag: CreateFileMapping failed with error %ld\n", GetLastError());
        CloseHandle(hFile);
        return false;
    }

    lpBasePtr = MapViewOfFile(
        hMap,
        FILE_MAP_READ,         // dwDesiredAccess
        0,                     // dwFileOffsetHigh
        0,                     // dwFileOffsetLow
        0);                    // dwNumberOfBytesToMap
    if (lpBasePtr == NULL) {
        LOG("popnhax: auto_diag: MapViewOfFile failed with error %ld\n", GetLastError());
        CloseHandle(hMap);
        CloseHandle(hFile);
        return false;
    }

    char *data = (char *)lpBasePtr;
    int32_t delta = 0;

    //first retrieve .rdata virtual and raw addresses to compute delta
    {
        int64_t string_loc = _search(data, liFileSize.QuadPart, ".rdata", 6, 0);
        if (string_loc == -1) {
            LOG("popnhax: auto_diag: could not retrieve .rdata section header\n");
            UnmapViewOfFile(lpBasePtr);
            CloseHandle(hMap);
            CloseHandle(hFile);
            return false;
        }
        uint32_t virtual_address = *(uint32_t *)(data + string_loc + 0x0C);
        uint32_t raw_address = *(uint32_t *)(data + string_loc + 0x14);
        delta = virtual_address - raw_address;
    }

    //now attempt to find music limit from the dll
    {
        int64_t string_loc = _search(data, liFileSize.QuadPart, "Illegal music no %d", 19, 0);
        if (string_loc == -1) {
            LOG("popnhax: auto_diag: could not retrieve music limit error string\n");
            UnmapViewOfFile(lpBasePtr);
            CloseHandle(hMap);
            CloseHandle(hFile);
            return false;
        }

        string_loc += delta; //convert to virtual address
        string_loc += 0x10000000; //entrypoint

        char *as_hex = (char *) &string_loc;
        int64_t pattern_offset = _search(data, liFileSize.QuadPart, as_hex, 4, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: auto_diag: could not retrieve music limit test function\n");
            UnmapViewOfFile(lpBasePtr);
            CloseHandle(hMap);
            CloseHandle(hFile);
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset - 0x1F;
        *limit = *(uint32_t*)patch_addr;
    }

    UnmapViewOfFile(lpBasePtr);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return true;
}

bool g_timer_flipflop = 0;

void (*real_timer_increase)(void);
void hook_timer_increase()
{
    __asm("cmp byte ptr [_g_timer_flipflop], 0\n");
    __asm("je skip_cancel_increase\n");
    __asm("dec dword ptr [ebp+0x44]\n");
    __asm("dec byte ptr [_g_timer_flipflop]\n"); // 1 -> 0
    __asm("jmp leave_timer_increase_hook\n");
    __asm("skip_cancel_increase:\n");
    __asm("inc byte ptr [_g_timer_flipflop]\n"); // 0 -> 1
    __asm("leave_timer_increase_hook:\n");
    real_timer_increase();
}

static bool patch_half_timer_speed()
{
    DWORD dllSize = 0;
    char *data = getDllData(g_game_dll_fn, &dllSize);

    {
        int64_t pattern_offset = _search(data, dllSize, "\xFF\x45\x44\x3B\x75\x04", 6, 0);
        if (pattern_offset == -1) {
            LOG("popnhax: high_framerate: cannot find timer increase function\n");
            return false;
        }

        uint64_t patch_addr = (int64_t)data + pattern_offset + 0x03;

        _MH_CreateHook((LPVOID)patch_addr, (LPVOID)hook_timer_increase,
                     (void **)&real_timer_increase);
    }
    LOG("popnhax: halve timer speed\n");
    return true;
}

static bool patch_afp_framerate(uint16_t fps)
{
    DWORD framerate = fps;

    if ( framerate == 0 )
    {
        DEVMODE lpDevMode;
        memset(&lpDevMode, 0, sizeof(DEVMODE));
        lpDevMode.dmSize = sizeof(DEVMODE);
        lpDevMode.dmDriverExtra = 0;

        if ( EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &lpDevMode) == 0 )
        {
            LOG("popnhax: high_framerate: could not retrieve display mode\n");
            return false;
        }

        framerate = lpDevMode.dmDisplayFrequency;
    } else {
        LOG("popnhax: high_framerate: force %ldHz\n", framerate);
    }

    float new_value = 1./framerate;
    char *as_hex = (char*)&new_value;

    if ( !find_and_patch_hex(g_game_dll_fn, "\x82\x9D\x88\x3C", 4, 0, as_hex, 4) )
    {
        LOG("popnhax: high_framerate: cannot patch animation speed for %ldHz\n", framerate);
        return false;
    }

    LOG("popnhax: high_framerate: patched animation speed for %ldHz\n", framerate);
    return true;
}

static bool patch_quick_boot()
{
    if ( !find_and_patch_hex(g_game_dll_fn, "\x8B\xF0\x8B\x16\x8B\x42\x04\x6A\x00", 9, -10, "\x90\x90\x90\x90\x90", 5) )
    {
        LOG("popnhax: quick_boot: cannot patch song checks\n");
        return false;
    }

    LOG("popnhax: quick boot enabled\n");
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        g_log_fp = fopen("popnhax.log", "w");
        if (g_log_fp == NULL)
        {
            LOG("cannot open popnhax.log for write, output to stderr only\n");
        }
        LOG("== popnhax version " PROGRAM_VERSION " ==\n");
        LOG("popnhax: Initializing\n");
        if (MH_Initialize() != MH_OK) {
            LOG("Failed to initialize minhook\n");
            exit(1);
            return TRUE;
        }

        bool force_trans_debug = false;
        bool force_no_omni     = false;

        LPWSTR *szArglist;
        int nArgs;

        szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
        if (szArglist == NULL) {
            LOG("popnhax: Failed to get cmdline\n");
            return 0;
        }

        for (int i = 0; i < nArgs; i++) {
            /* game dll */
            if ( wcsstr(szArglist[i], L".dll") != NULL && wcsstr(szArglist[i], L"popn2") == szArglist[i] )
            {
                char* resultStr = new char [wcslen(szArglist[i]) + 1];
                wsprintfA ( resultStr, "%S", szArglist[i]);
                g_game_dll_fn = strdup(resultStr);
            }
            /* config xml */
            else if ( wcscmp(szArglist[i], L"--popnhax-config") == 0 )
            {
                char* resultStr = new char [wcslen(szArglist[i+1]) + 1];
                wsprintfA ( resultStr, "%S", szArglist[i+1]);
                g_config_fn = strdup(resultStr);
            }
            else if ( wcscmp(szArglist[i], L"--translation-debug") == 0 )
            {
                LOG("--translation-debug: turning on translation-related dumps\n");
                force_trans_debug = true;
            }
            else if ( wcscmp(szArglist[i], L"--no-omni") == 0 )
            {
                LOG("--no-omni: force disable patch_db\n");
                force_no_omni = true;
            }
        }
        LocalFree(szArglist);

        if (g_game_dll_fn == NULL)
            g_game_dll_fn = strdup("popn22.dll");


        uint8_t game_version = get_version();

        LOG("popnhax: game dll: %s ",g_game_dll_fn);
        if ( game_version != 0 )
            LOG ("(popn%d)", game_version);
        LOG("\n");

        if (g_config_fn == NULL)
        {
            /* if there's an xml named like the custom game dll, it takes priority */
            char *tmp_name = strdup(g_game_dll_fn);
            strcpy(tmp_name+strlen(tmp_name)-3, "xml");
            if (access(tmp_name, F_OK) == 0)
                g_config_fn = strdup(tmp_name);
            else
                g_config_fn = strdup("popnhax.xml");
            free(tmp_name);
        }

        LOG("popnhax: config file: %s\n",g_config_fn);

        if ( !config_process(g_config_fn) )
        {
            LOG("FATAL ERROR: Could not pre-process config file\n");
            exit(1);
        }

        strcpy(g_config_fn+strlen(g_config_fn)-3, "opt");

        if (!_load_config(g_config_fn, &config, config_psmap))
        {
            LOG("popnhax: FATAL ERROR: failed to load %s. Running advanced diagnostic...\n", g_config_fn);
            config_diag(g_config_fn, config_psmap);
            exit(1);
        }

        config.game_version = game_version;

        if ( config.extended_debug )
        {
            enable_extended_debug();
        }

        if ( strcmp(g_game_dll_fn, "popn22.dll") == 0 )
        {
            //ensure you're not running popn22.dll from the modules subfolder
            char filename[MAX_PATH];
            if ( GetModuleFileName(GetModuleHandle(g_game_dll_fn), filename, MAX_PATH+1) != 0 )
            {
                if ( strstr(filename, "\\modules\\popn22.dll") != NULL )
                {
                    LOG("WARNING: running popn22.dll from \"modules\" subfolder is not recommended with popnhax. Please copy dlls back to contents folder\n");
                }
            } else {
                LOG("WARNING: auto_diag: Cannot retrieve module path (%ld)\n", GetLastError());
            }

            //ensure there isn't a more recent version in modules subfolder
            uint32_t modules_limit, current_limit;
            if ( get_music_limit(&current_limit)
              && get_music_limit_from_file("modules\\popn22.dll", &modules_limit)
              && (modules_limit > current_limit) )
            {
                LOG("ERROR: a newer version of popn22.dll seems to be present in modules subfolder (%d vs %d songs). Please copy dlls back to contents folder\n", modules_limit, current_limit);
            }
        }

        if (force_trans_debug)
            config.translation_debug = true;

        if (force_no_omni)
            config.patch_db = false;

        bool datecode_auto = (strcmp(config.force_datecode, "auto") == 0);

        if (!config.disable_multiboot)
        {
            /* automatically force datecode based on dll name when applicable (e.g. popn22_2022061300.dll and force_datecode is empty or "auto") */
            if ( (strlen(g_game_dll_fn) == 21)
              && ( datecode_auto || (config.force_datecode[0] == '\0') ) )
            {
                LOG("popnhax: multiboot autotune activated (custom game dll, no force_datecode)\n");
                memcpy(config.force_datecode, g_game_dll_fn+7, 10);
                LOG("popnhax: multiboot: auto set datecode to %s\n", config.force_datecode);
                if (config.score_challenge && ( config.game_version < 26 || strcmp(config.force_datecode,"2020092800") <= 0 ) )
                {
                    LOG("popnhax: multiboot: auto disable score challenge patch (already ingame)\n");
                    config.score_challenge = false;
                }
                if (config.patch_db && ( config.game_version == 0 || strcmp(config.force_datecode,"2016121400") < 0 ) )
                {
                    LOG("popnhax: multiboot: auto disable omnimix patch (not compatible)\n");
                    config.patch_db = false;
                }
                if (config.guidese_off && ( config.game_version == 0 || strcmp(config.force_datecode,"2016121400") < 0 ) )
                {
                    LOG("popnhax: multiboot: auto disable Guide SE patch (not compatible)\n");
                    config.guidese_off = false;
                }
                if (config.local_favorites && ( config.game_version == 0 || strcmp(config.force_datecode,"2016121400") < 0 ) )
                {
                    LOG("popnhax: multiboot: auto disable local favorites patch (not compatible)\n");
                    config.local_favorites = false;
                }
                if ((config.tachi_scorehook || config.tachi_rivals) && ( config.game_version < 24 || strcmp(config.force_datecode,"2016120400") <= 0 ) )
                {
                    LOG("popnhax: multiboot: auto disable tachi hooks (not supported)\n");
                    config.tachi_scorehook = false;
                    config.tachi_rivals = false;
                }
            }
        }

        if (config.force_datecode[0] != '\0')
        {
            if (!datecode_auto && strlen(config.force_datecode) != 10)
                LOG("popnhax: force_datecode: Invalid datecode %s, should be 10 digits (e.g. 2022061300) or \"auto\"\n", config.force_datecode);
            else
                patch_datecode(config.force_datecode);
        }

        /* look for possible translation patch folder ("_yyyymmddrr_tr" for multiboot, or simply "_translation") */
        if (!config.disable_translation)
        {
            char translation_folder[16] = "";
            char translation_path[64] = "";

            /* parse */
            if ( g_datecode_override != NULL )
            {

                sprintf(translation_folder, "_%s%s", g_datecode_override, "_tr");
                sprintf(translation_path, "%s%s", "data_mods\\", translation_folder);
                if (access(translation_path, F_OK) != 0)
                {
                    translation_folder[0] = '\0';
                }
            }

            if (translation_folder[0] == '\0')
            {
                sprintf(translation_folder, "%s", "_translation");
                sprintf(translation_path, "%s%s", "data_mods\\", translation_folder);
                if (access(translation_path, F_OK) != 0)
                {
                    translation_folder[0] = '\0';
                }
            }

            if (translation_folder[0] != '\0')
            {
                LOG("popnhax: translation: using folder \"%s\"\n", translation_folder);
                patch_translate(g_game_dll_fn, translation_folder, config.translation_debug);
            }
            else if ( config.translation_debug )
            {
                DWORD dllSize = 0;
                char *data = getDllData(g_game_dll_fn, &dllSize);
                LOG("popnhax: translation debug: no translation applied, dump prepatched dll\n");
                FILE* dllrtp = fopen("dllruntime_prepatched.dll", "wb");
                fwrite(data, 1, dllSize, dllrtp);
                fclose(dllrtp);
            }
        }

/* ----------- r2nk226 ----------- */
        if (config.practice_mode) {
            version_check();
            patch_practice_mode();
            // record_mode must be usaneko or later
            if (p_version >= 4) {
                patch_record_mode(config.quick_retire);
            }
        }
/* --------------------------------- */

        if (config.audio_source_fix) {
            patch_audio_source_fix();
        }

        if (config.unset_volume) {
            patch_unset_volume();
        }

        if (config.pfree) {
            g_pfree_mode = true; /* used by asm hook */
            patch_pfree();
        }

        if (config.quick_retire) {
            patch_quick_retire(config.pfree);
        } else if (config.back_to_song_select) {
            LOG("WARNING: back to song select cannot be enabled without quick retire.\n");
        }

        if (config.score_challenge) {
            patch_score_challenge();
        }

        if (config.force_hd_timing) {
            patch_hd_timing();
        }

        if (config.force_hd_resolution) {
            patch_hd_resolution(config.force_hd_resolution);
        }

        if (config.iidx_hard_gauge){
            if (config.survival_gauge || config.survival_spicy || config.survival_iidx)
            {
                LOG("popnhax: iidx_hard_gauge cannot be used when other survival options are already set\n");
                config.iidx_hard_gauge = false;
            }
            else
            {
                config.survival_gauge = 3;
                config.survival_spicy = true;
                config.survival_iidx = true;
            }
        }

        if (config.survival_gauge) {
            bool res = true;
            res &= patch_hard_gauge_survival(config.survival_gauge);

            if (config.survival_spicy) {
                res &= patch_survival_spicy();
            }

            if (config.survival_iidx) {
                res &= patch_survival_iidx();
            }

            if (config.iidx_hard_gauge && res)
                LOG("popnhax: iidx_hard_gauge: HARD gauge is now IIDX-like\n");
        }

        if (config.hidden_is_offset){
            patch_hidden_is_offset();
            if (config.show_offset){
                patch_show_hidden_adjust_result_screen();
            }
        }

        if (config.show_fast_slow){
            force_show_fast_slow();
        }

        if (config.show_details){
            force_show_details_result();
        }

        if (config.audio_offset){
            if (config.keysound_offset)
            {
                LOG("popnhax: audio_offset cannot be used when keysound_offset is already set\n");
            }
            else
            {
                config.disable_keysounds = true;
                config.keysound_offset = -1*config.audio_offset;
                LOG("popnhax: audio_offset: disable keysounds then offset timing by %d ms\n", config.keysound_offset);
            }
        }

        if (config.disable_keysounds){
            patch_disable_keysound();
        }

        if (config.base_offset){
            patch_add_to_base_offset(config.base_offset);
        }

        if (config.keysound_offset){
            /* must be called _after_ force_hd_timing and base_offset */
            patch_keysound_offset(config.keysound_offset);
        }

        if (config.beam_brightness){
            /* must be called _after_ force_hd_resolution */
            patch_add_to_beam_brightness(config.beam_brightness);
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
            /* must be called after force_datecode */
            LOG("popnhax: patching songdb\n");
            if ( patch_database() )
            {
                patch_db_power_points();
                patch_db_fix_cursor();
                if (config.custom_categ)
                    patch_custom_categs(g_game_dll_fn, &config);
            }
        }

        if (config.force_unlocks) {
            if (!config.patch_db) {
                /* Only unlock using these methods if it's not done directly through the database hooks */
                force_unlock_songs();
                force_unlock_charas();
            }
            patch_unlocks_offline();
            force_unlock_deco_parts();
        }

        if (config.local_favorites)
        {
            if ( config.game_version == 0 )
            {
                LOG("popnhax: local_favorites: patch is not compatible with your game version.\n");
            }
            else
            {
                if ( strlen(config.local_favorites_path) > 0 )
                {
                    while ( config.local_favorites_path[strlen(config.local_favorites_path)-1] == '\\' )
                    {
                        config.local_favorites_path[strlen(config.local_favorites_path)-1] = '\0';
                    }
                    if (access(config.local_favorites_path, F_OK) == 0)
                        LOG("popnhax: local_favorites: favorites are stored in %s\n", config.local_favorites_path);
                    else
                    {
                        LOG("WARNING: local_favorites: cannot access %s, defaulting to data_mods folder\n", config.local_favorites_path);
                        config.local_favorites_path[0] = '\0';
                    }
                }

                uint8_t *datecode = NULL;
                if ( g_datecode_override != NULL )
                {
                    datecode = (uint8_t*) strdup(g_datecode_override);
                }
                else
                {
                    property *config_xml = load_prop_file("prop/ea3-config.xml");
                    READ_STR_OPT(config_xml, property_search(config_xml, NULL, "/ea3/soft"), "ext", datecode)
                    free(config_xml);
                }

                if (datecode == NULL) {
                    LOG("popnhax: local_favorites: failed to retrieve datecode.\n");
                }

                patch_local_favorites(g_game_dll_fn, config.game_version, config.local_favorites_path, ( datecode != NULL && strcmp((char*)datecode,"2023101700") >= 0 ) );
                free(datecode);

            }
        }

        if (config.force_full_opt)
            option_full();

        if (config.netvs_off)
            option_net_ojama_off();

        if (config.guidese_off)
            option_guide_se_off();

        if (config.high_framerate)
        {
            patch_afp_framerate(config.high_framerate_fps);
            patch_half_timer_speed();
            config.fps_uncap = true;
        } else if (config.force_slow_timer) {
            patch_half_timer_speed();
        }

        if (config.fps_uncap)
            patch_fps_uncap(config.high_framerate_limiter ? config.high_framerate_fps : 0);

        if (config.enhanced_polling)
        {
            // setup hardware offload if applicable
            g_hardware_offload = patch_enhanced_polling_hardware_setup();

            patch_enhanced_polling(config.debounce, config.enhanced_polling_stats);
            if (config.enhanced_polling_stats)
            {
                patch_enhanced_polling_stats();
            }
            if (g_hardware_offload)
                patch_usbio_string();
        }

        if (config.hispeed_auto)
        {
            g_default_bpm = config.hispeed_default_bpm;
            patch_hispeed_auto(config.hispeed_auto);
        }

        if (config.practice_mode && config.tachi_scorehook)
        {
            LOG("WARNING: tachi: scorehook not compatible with practice mode, disabling it.\n");
            config.tachi_scorehook = false;
        }

        if (config.tachi_scorehook || config.tachi_rivals)
        {
            SearchFile s;
            bool found = false;
            s.search(".", "conf", false);
            auto result = s.getResult();

            if ( result.size() > 0 )
            {
                for (uint16_t i=0; i<result.size(); i++) {
                    if (strstr(result[i].c_str(), ".\\_tachi.") == result[i].c_str())
                    {
                        found = true;
                        break;
                    }
                }
            }

            if (!found)
            {
                LOG("WARNING: tachi: no config file found for tachi hooks ( _tachi.default.conf or _tachi.<friendid>.conf ), disabling them.\n");
                config.tachi_scorehook = false;
                config.tachi_rivals = false;
            }
        }

        if (config.tachi_scorehook)
        {
            patch_tachi_scorehook(g_game_dll_fn, config.pfree, config.hidden_is_offset, config.tachi_scorehook_skip_omni);
        }

        if (config.tachi_rivals)
        {
            //must be called after scorehook
            patch_tachi_rivals(g_game_dll_fn, config.tachi_scorehook);
        }

        if (config.autopin)
        {
            patch_autopin();
        }

        if (config.attract_interactive)
        {
            patch_attract_interactive();
        }

        if (config.attract_lights)
        {
            patch_attract_lights();
        }

        if (config.attract_ex)
        {
            patch_ex_attract( config.hispeed_auto ? config.hispeed_default_bpm : 0 );
        }

        if (config.attract_full)
        {
            patch_full_attract();
        }

        if (config.quick_boot)
        {
            patch_quick_boot();
        }

        if (config.time_rate)
            patch_get_time(config.time_rate/100.);

        MH_EnableHook(MH_ALL_HOOKS);

        LOG("popnhax: done patching game, enjoy!\n");

        if (g_log_fp != stderr)
            fclose(g_log_fp);

        break;
    }

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}
