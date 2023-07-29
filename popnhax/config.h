#ifndef __POPNHAX_CONFIG__
#define __POPNHAX_CONFIG__

#include <stdbool.h>

struct popnhax_config {
    bool practice_mode;
    bool hidden_is_offset;
    bool show_offset;
    bool show_fast_slow;
    bool pfree;
    bool quick_retire;
    bool back_to_song_select;
    bool score_challenge;
    bool force_hd_timing;
    uint8_t force_hd_resolution;
    bool force_unlocks;
    bool audio_source_fix;
    bool unset_volume;
    bool event_mode;
    bool remove_timer;
    bool freeze_timer;
    bool skip_tutorials;
    bool force_full_opt;

    bool patch_db;
    bool disable_expansions;
    bool disable_redirection;
    bool disable_multiboot;
    bool patch_xml_auto;
    char patch_xml_filename[MAX_PATH];
    char force_datecode[11];
    bool network_datecode;
    int8_t audio_offset;
    bool disable_keysounds;
    int8_t keysound_offset;
    int8_t beam_brightness;
    bool fps_uncap;
    bool disable_translation;
    bool translation_debug;
    bool enhanced_polling;
    uint8_t debounce;
    bool enhanced_polling_stats;
    int8_t enhanced_polling_priority;
};

#endif
