#ifndef __SEARCH_H__
#define __SEARCH_H__

int search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);
int search_debug(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);
int wildcard_search(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);
int wildcard_search_debug(char *haystack, size_t haystack_size, const char *needle, size_t needle_size, size_t orig_offset);

#endif
