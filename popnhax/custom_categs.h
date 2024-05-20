#ifndef __CUSTOM_CATEGS_H__
#define __CUSTOM_CATEGS_H__

#include <stdint.h>
#include "popnhax/config.h"
#include "util/bst.h"

extern uint32_t g_max_id;
extern bst_t *g_customs_bst;

bool is_excluded_folder(const char *input_filename);
bool patch_custom_categs(const char *dllFilename, struct popnhax_config *config);
bool patch_local_favorites(const char *dllFilename, uint8_t version);

#endif
