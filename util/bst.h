#ifndef __BST_H__
#define __BST_H__

typedef struct bst_s
{
    uint32_t data;
    struct bst_s *right;
    struct bst_s *left;
} bst_t;

bst_t* bst_search(bst_t *root, uint32_t val);
bst_t* bst_insert(bst_t *root, uint32_t val);

#endif
