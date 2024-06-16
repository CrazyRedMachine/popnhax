#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "bst.h"

bst_t* bst_search(bst_t *root, uint32_t val)
{
    if( root == NULL || root->data == val )
        return root;
    else if( val > (root->data) )
        return bst_search(root->right, val);
    else
        return bst_search(root->left,val);
}

bst_t* bst_insert_ex(bst_t *root, uint32_t val, void* extra_data, void*(*extra_data_cb)(void *arg1, void *arg2))
{
    if ( root == NULL )
    {
        bst_t *p;
        p = (bst_t *)malloc(sizeof(bst_t));
        p->data = val;
        p->left = NULL;
        p->right = NULL;
        p->extra_data = extra_data;
        return p;
    }
    else if ( extra_data != NULL && extra_data_cb != NULL && val == root->data )
    {
        //already existing, process extra_data
        root->extra_data = extra_data_cb(root->extra_data, extra_data);
    }
    else if ( val > root->data )
        root->right = bst_insert_ex(root->right, val, extra_data, extra_data_cb);
    else
        root->left = bst_insert_ex(root->left, val, extra_data, extra_data_cb);

    return root;
}

bst_t* bst_insert(bst_t *root, uint32_t val)
{
    return bst_insert_ex(root, val, NULL, NULL);
}