#ifndef __ATTRACT_H__
#define __ATTRACT_H__

#include <stdint.h>
#include "popnhax/config.h"

bool patch_attract_interactive();
bool patch_attract_lights();
bool patch_ex_attract(uint16_t target_bpm);
bool patch_full_attract();

#endif
