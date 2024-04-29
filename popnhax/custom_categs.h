#ifndef __CUSTOM_CATEGS_H__
#define __CUSTOM_CATEGS_H__

#include <stdint.h>
#include "popnhax/config.h"

bool patch_custom_categs(const char *dllFilename, struct popnhax_config *config);
bool patch_local_favorites(const char *dllFilename);

#endif
