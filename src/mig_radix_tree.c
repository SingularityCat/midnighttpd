#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mig_radix_tree.h"

struct mig_radix_node *mig_radix_node_create()
{
    struct mig_radix_node *node = calloc(sizeof(struct mig_radix_node), 1);
    return node;
}

void mig_radix_node_destroy(struct mig_radix_node *node)
{
    if(node->segment)
    {
        free(node->segment);
    }

    for(int i = 0; i < 256; i++)
    {
        if(node->nodes[i])
        {
            mig_radix_node_destroy(node->nodes[i]);
        }
    }
    free(node);
}

struct mig_radix_node **mig_radix_node_find(struct mig_radix_node **root, const uint8_t *key, size_t klen,
                                            size_t *koffp, size_t *soffp, struct mig_radix_node **prevp)
{
    uint8_t seg;
    size_t koff = 0, soff;

    struct mig_radix_node *prev = NULL, *cur = *root, **curp = root;

    while(cur != NULL)
    {
        soff = 0;
        while(koff < klen && soff < cur->seglen && key[koff] == cur->segment[soff])
        {
            koff++;
            soff++;
        }

        if(soff == cur->seglen && koff < klen)
        {
            curp = &cur->nodes[key[koff++]];
            prev = cur;
            cur = *curp;
        }
        else
        {
            cur = NULL;
        }
    }

    if(koffp)
    {
        *koffp = koff;
    }
    if(soffp)
    {
        *soffp = soff;
    }
    if(prevp)
    {
        *prevp = prev;
    }
    return curp;
}

struct mig_radix_tree *mig_radix_tree_create()
{
    struct mig_radix_tree *tree = malloc(sizeof(struct mig_radix_tree));
    tree->root = NULL;
    return tree;
}

void mig_radix_tree_destroy(struct mig_radix_tree *tree)
{
    if(tree->root)
    {
        mig_radix_node_destroy(tree->root);
    }
    free(tree);
}

void mig_radix_tree_insert(struct mig_radix_tree *tree, const uint8_t *key, size_t klen, void *value)
{
    uint8_t seg;
    size_t koffset, soffset;
    struct mig_radix_node *new, **lmp = mig_radix_node_find(&tree->root, key, klen, &koffset, &soffset, NULL);

    new = mig_radix_node_create();
    if(!new)
    {
        return;
    }

    if(koffset < klen)
    {
        if(*lmp != NULL && (*lmp)->seglen > soffset)
        {
            new->seglen = soffset;
            new->segment = malloc(new->seglen);
            memcpy(new->segment, (*lmp)->segment, new->seglen);
            new->nodes[(*lmp)->segment[soffset++]] = *lmp;
            (*lmp)->seglen -= soffset;
            memmove((*lmp)->segment, (*lmp)->segment + soffset, (*lmp)->seglen);
            *lmp = new;
            lmp = &new->nodes[key[koffset++]];
            new = mig_radix_node_create();
        }

        new->seglen = klen - koffset;
        new->segment = malloc(new->seglen);
        memcpy(new->segment, key + koffset, new->seglen);
        new->value = value;
        *lmp = new;
    }
    else
    {
        (*lmp)->value = value;
    }
}

void *mig_radix_tree_lookup(struct mig_radix_tree *tree, const uint8_t *key, size_t klen)
{
    size_t soffset;
    struct mig_radix_node *lkn = *mig_radix_node_find(&tree->root, key, klen, NULL, &soffset, NULL);
    return lkn && soffset == lkn->seglen ? lkn->value : NULL;
}

void *mig_radix_tree_lpm(struct mig_radix_tree *tree, const uint8_t *key, size_t klen)
{
    size_t soffset;
    struct mig_radix_node *prp, *lkn = *mig_radix_node_find(&tree->root, key, klen, NULL, &soffset, &prp);
    if(!lkn || soffset < lkn->seglen)
    {
        lkn = prp;
    }
    return lkn ? lkn->value : NULL;
}

