#ifndef __BST_H__
#define __BST_H__

#include <stdint.h>

typedef struct bst_s
{
    uint32_t data;
    void *extra_data;
    struct bst_s *right;
    struct bst_s *left;
} bst_t;

bst_t* bst_search(bst_t *root, uint32_t val);
bst_t* bst_insert(bst_t *root, uint32_t val);
bst_t* bst_insert_ex(bst_t *root, uint32_t val, void *extra_data, void*(*extra_data_cb)(void *arg1, void *arg2));

#endif
