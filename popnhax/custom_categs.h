#ifndef __CUSTOM_CATEGS_H__
#define __CUSTOM_CATEGS_H__

#include <stdint.h>
#include "popnhax/config.h"

extern uint32_t g_max_id;

bool is_excluded_folder(const char *input_filename);
bool patch_custom_categs(const char *dllFilename, struct popnhax_config *config);
bool patch_local_favorites(const char *dllFilename, uint8_t version);

#endif
