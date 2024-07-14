#ifndef __TACHI_H__
#define __TACHI_H__

#include <stdint.h>

bool patch_tachi_scorehook(const char *dllFilename, bool pfree, bool hidden_is_offset, bool skip_omni);
bool patch_tachi_rivals(const char *dllFilename, bool scorehook);

#endif
