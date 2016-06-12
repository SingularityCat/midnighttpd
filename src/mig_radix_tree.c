#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
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
    size_t koffset, soffset;
    struct mig_radix_node *new, **lmp = mig_radix_node_find(&tree->root, key, klen, &koffset, &soffset, NULL);

    /* There are three possible cases we need to handle:
     * 1) A key is a superset of an existing key path. e.g:
     *      [root] --nav--> [null]
     *    Adding "navigation",
     *      [root] --nav--> i --gation--> [null]
     *
     * 2) A key is a subset of an existing key path. e.g:
     *      [root] --navigation--> [null]
     *    Adding "nav",
     *      [root] --nav--> i --gation--> [null]
     *
     * 3) A key intersects with an existing key path. e.g:
     *      [root] --nav--> i --gation--> [null]
     *    Adding "navigator",
     *      [root] --nav--> i --gat--> i --on--> [null]
     *                             \-> o --r--> [null]
     *
     * There is also the case where a key has a perfect match.
     * This implies the key is being updated (re-added),
     * so no tree transformations are necessary.
     *
     * In case 1, key offset < key length, and the node pointer is null
     * In case 2, key offset = key length, but segment offset < segment length
     * In case 3, key offset < key length, and segment offset < segment length
     *
     * And in the perfect match case, both offsets equal their respective length.
     */

    bool kokl = koffset < klen;
    bool sosl = *lmp != NULL ? soffset < (*lmp)->seglen : false;

    if(kokl || sosl)
    {
        new = mig_radix_node_create();
        if(!new) { return; }

        if(sosl)
        {
            /* new is the splitting node. */
            new->seglen = soffset;
            new->segment = malloc(new->seglen);
            /* copy the shared part of the key into it. */
            memcpy(new->segment, (*lmp)->segment, new->seglen);
            /* make old node a child of the new node, increment segment offset */
            new->nodes[(*lmp)->segment[soffset++]] = *lmp;
            /* remove the shared part of the original node. */
            (*lmp)->seglen -= soffset;
            memmove((*lmp)->segment, (*lmp)->segment + soffset, (*lmp)->seglen);
            /* replace old node pointer with new node pointer. */
            *lmp = new;
            if(kokl)
            {
                /* If there is still some of the key left,
                 * create another node and update node pointer pointer
                 */
                lmp = &new->nodes[key[koffset++]];
                new = mig_radix_node_create();
                if(!new) { return; }
            }
        }

        if(kokl)
        {
            new->seglen = klen - koffset;
            new->segment = malloc(new->seglen);
            memcpy(new->segment, key + koffset, new->seglen);
        }
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

