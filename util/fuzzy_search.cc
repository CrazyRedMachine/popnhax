#include <memory.h>
#include <stdint.h>

#include "util/fuzzy_search.h"

int find_block_core(char *haystack, size_t haystack_size, fuzzy_search_task *needle,
                    size_t orig_offset, int dir) {
    size_t offset = orig_offset;
    haystack_size += orig_offset;
    while (offset + 1 < orig_offset + haystack_size) {
        size_t offset_temp = offset;
        int found = 1;

        if (needle->count <= 0) {
            found = 0;
        }

        for (int i = 0; i < needle->count; i++) {
            int subfound = -1;

            if (needle->blocks[i].type == 1) {
                offset_temp += needle->blocks[i].length;
                continue;
            }

            for (int j = 0; j < needle->blocks[i].count; j++) {
                if (haystack + offset_temp + needle->blocks[i].data[j].length <
                        haystack + haystack_size &&
                    memcmp(haystack + offset_temp, needle->blocks[i].data[j].block,
                           needle->blocks[i].data[j].length) == 0) {
                    subfound = j;
                    break;
                }
            }

            if (subfound == -1) {
                found = 0;
                break;
            }

            offset_temp += needle->blocks[i].data[subfound].length;
        }

        if (found == 1) {
            return offset;
        } else {
            offset += dir;
        }
    }

    return -1;
}

int find_block(char *haystack, size_t haystack_size, fuzzy_search_task *needle,
               size_t orig_offset) {
    return find_block_core(haystack, haystack_size, needle, orig_offset, 1);
}

int find_block_back(char *haystack, size_t haystack_size, fuzzy_search_task *needle,
                    size_t orig_offset) {
    return find_block_core(haystack, haystack_size, needle, orig_offset, -1);
}