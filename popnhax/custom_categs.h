#ifndef __CUSTOM_CATEGS_H__
#define __CUSTOM_CATEGS_H__

#include <stdint.h>
#include "popnhax/config.h"
#include "util/bst.h"

typedef struct {
    char *name;
    uint32_t size;
    uint32_t *songlist; //really is a (songlist_t *) pointer
} subcategory_s;

extern uint32_t g_max_id;
extern bst_t *g_customs_bst;

void init_subcategories();
void add_song_to_subcateg(uint32_t songid, subcategory_s* subcateg);
subcategory_s* get_subcateg(const char *title);

bool is_a_custom(uint32_t songid);
bool patch_custom_categs(const char *dllFilename, struct popnhax_config *config);
bool patch_local_favorites(const char *dllFilename, uint8_t version, char *fav_folder, bool with_numpad9_patch);

#endif
