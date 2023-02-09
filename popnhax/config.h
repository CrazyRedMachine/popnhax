#ifndef __POPNHAX_CONFIG__
#define __POPNHAX_CONFIG__

#include <stdbool.h>

struct popnhax_config {
    bool pfree;
    uint8_t hd_on_sd;
    bool force_unlocks;
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
};

#endif
