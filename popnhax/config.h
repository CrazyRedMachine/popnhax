#ifndef __POPNHAX_CONFIG__
#define __POPNHAX_CONFIG__

#include <stdbool.h>

struct popnhax_config {
    bool hidden_is_offset;
    bool pfree;
    bool quick_retire;
    bool force_hd_timing;
    uint8_t force_hd_resolution;
    bool force_unlocks;
    bool force_unlock_deco;
    bool unset_volume;
    bool event_mode;
    bool remove_timer;
    bool freeze_timer;
    bool skip_tutorials;

    bool patch_db;
    bool disable_expansions;
    bool disable_redirection;
    bool patch_xml_auto;
    char patch_xml_filename[MAX_PATH];
    int8_t keysound_offset;
    int8_t beam_brightness;
};

#endif
