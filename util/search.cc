#include <memory.h>
#include <stdint.h>
#include <cstdio>

#include "util/log.h"

#define NO_OF_CHARS 256

// A utility function to get maximum of two integers
static int max(int a, int b) {
    return (a > b) ? a : b;
}

// The preprocessing function for Boyer Moore's bad character heuristic
static void badCharHeuristic(const unsigned char *str, int size, int* badchar) {
    int i;

    // Initialize all occurrences as -1
    for (i = 0; i < NO_OF_CHARS; i++)
        badchar[i] = -1;

    // Fill the actual value of last occurrence of a character
    for (i = 0; i < size; i++)
        badchar[(int) str[i]] = i;
}

#define DEBUG_SEARCH 0

int _search(unsigned char *haystack, size_t haystack_size, const unsigned char *needle, size_t needle_size, int orig_offset, int debug) {
    int badchar[NO_OF_CHARS];

    badCharHeuristic(needle, needle_size, badchar);

   int64_t s = 0; // s is shift of the pattern with respect to text
    while (s <= (haystack_size - needle_size)) {
        int j = needle_size - 1;
 if (debug == 2)
 {
        LOG("--------------------------------\n");
        LOG("txt...");
        for (size_t i = 0; i < needle_size; i++)
        {
            LOG("%02x ", haystack[orig_offset+s+i]);
        }
        LOG("\n");
        LOG("pat...");
        for (size_t i = 0; i < needle_size; i++)
        {
            LOG("%02x ", needle[i]);
        }
        LOG("\n");
 }
        while (j >= 0 && needle[j] == haystack[orig_offset + s + j])
            j--;

        if (j < 0) {
                if (debug)
                LOG("found string at offset %llx!\n", orig_offset +s);
            return orig_offset + s;
        }
        else
        {
            s += max(1, j - badchar[(int)haystack[orig_offset + s + j]]);
            if (debug)
                LOG("mismatch at pos %d, new offset %llx\n\n", j, orig_offset+s);
        }
    }

    return -1;
}

int search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset) {
    int res = _search((unsigned char*) haystack, haystack_size, (const unsigned char *)needle, needle_size, orig_offset, 0);
    return res;
}

int search_debug(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset) {
    int res = _search((unsigned char*) haystack, haystack_size, (const unsigned char *)needle, needle_size, orig_offset, 2);
    return res;
}