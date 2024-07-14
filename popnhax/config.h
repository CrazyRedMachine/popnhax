#ifndef __POPNHAX_CONFIG__
#define __POPNHAX_CONFIG__

#include <stdbool.h>

struct popnhax_config {
    uint8_t game_version;
    bool practice_mode;
    bool hidden_is_offset;
    bool iidx_hard_gauge;
    bool show_offset;
    bool show_fast_slow;
    bool show_details;
    bool pfree;
    bool quick_retire;
    bool back_to_song_select;
    bool score_challenge;
    uint8_t custom_categ;
    uint16_t custom_categ_min_songid;
    bool custom_exclude_from_version;
    bool custom_exclude_from_level;
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
    bool netvs_off;
    bool guidese_off;
    bool local_favorites;
    char local_favorites_path[MAX_PATH-18]; // xx.yyyyyyyyyyyy.fav
    bool tachi_scorehook;
    bool tachi_scorehook_skip_omni;
    bool tachi_rivals;
    bool autopin;

    bool patch_db;
    bool disable_multiboot;
    bool patch_xml_dump;
    bool ignore_music_limit;
    char force_patch_xml[MAX_PATH+1];
    char force_datecode[11];
    bool network_datecode;
    int8_t audio_offset;
    bool disable_keysounds;
    int8_t keysound_offset;
    int8_t beam_brightness;
    bool fps_uncap;
    bool disable_translation;
    bool enhanced_polling;
    uint8_t debounce;
    bool enhanced_polling_stats;
    int8_t enhanced_polling_priority;
    uint8_t hispeed_auto;
    uint16_t hispeed_default_bpm;
    int8_t base_offset;
    char custom_category_title[16];
    char custom_category_format[64];
    char custom_track_title_format[64];
    char custom_track_title_format2[64];
    bool exclude_omni;
    bool partial_entries;
    bool high_framerate;
    bool high_framerate_limiter;
    uint16_t high_framerate_fps;

    /* removed/hidden */
    bool disable_expansions;
    bool disable_redirection;
    uint8_t survival_gauge;
    bool survival_iidx;
    bool survival_spicy;
    bool translation_debug;
    uint16_t time_rate;
};

bool config_process(const char *filepath); // take care of updating .xml/.opt files if needed
void config_diag(const char *xml_filepath, const struct property_psmap *psmap);

#endif
