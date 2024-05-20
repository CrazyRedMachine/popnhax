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

bst_t* bst_insert(bst_t *root, uint32_t val)
{
    if ( root == NULL )
    {
        bst_t *p;
        p = (bst_t *)malloc(sizeof(bst_t));
        p->data = val;
        p->left = NULL;
        p->right = NULL;
        return p;
    }
    else if ( val > root->data )
        root->right = bst_insert(root->right, val);
    else
        root->left = bst_insert(root->left, val);

    return root;
}