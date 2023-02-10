#ifndef __FUZZY_SEARCH_H__
#define __FUZZY_SEARCH_H__

#include <stddef.h>

#define FUZZY_START(task, _count)                                                                  \
    {                                                                                              \
        task.count = _count;                                                                       \
        task.blocks = (fuzzy_search_block *)calloc(task.count, sizeof(fuzzy_search_block));        \
    }

#define FUZZY_CODE(task, id, code, len)                                                            \
    {                                                                                              \
        task.blocks[id].count = 1;                                                                 \
        task.blocks[id].data[0].type = 0;                                                          \
        task.blocks[id].data[0].length = len;                                                      \
        task.blocks[id].data[0].block =                                                            \
            (char *)calloc(task.blocks[id].data[0].length, sizeof(char));                          \
        memcpy(task.blocks[id].data[0].block, code, task.blocks[id].data[0].length);               \
    }

#define FUZZY_WILDCARD(task, id, len)                                                              \
    {                                                                                              \
        task.blocks[id].count = 1;                                                                 \
        task.blocks[id].type = 1;                                                                  \
        task.blocks[id].length = len;                                                              \
        task.blocks[id].data[0].length = len;                                                      \
    }

#define FUZZY_SIZE(task, id) (task.blocks[id].data[0].length)

typedef struct fuzzy_search_block_data {
    int type;
    int length;
    char *block;
} fuzzy_search_block_data;

typedef struct fuzzy_search_block {
    int type;
    int length;
    int count;
    fuzzy_search_block_data data[0x10];
} fuzzy_search_block;

typedef struct fuzzy_search_task {
    int count;
    fuzzy_search_block *blocks;
} fuzzy_search_task;

int find_block(char *haystack, size_t haystack_size, fuzzy_search_task *needle, size_t offset);
int find_block_back(char *haystack, size_t haystack_size, fuzzy_search_task *needle, size_t offset);

#endif
