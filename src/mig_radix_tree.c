#include <stdlib.h>
#include <string.h>

#include "mig_radix_tree.h"

struct mig_radix_node *mig_radix_node_create()
{
    struct mig_radix_node *node = calloc(sizeof(struct mig_radix_node), 1);
    return node;
}

void mig_radix_node_destroy(struct mig_radix_node *node)
{
    free(node);
}

struct mig_radix_node **mig_radix_node_find(struct mig_radix_node **root, uint8_t *key, size_t klen, size_t *koffp, size_t *soffp)
{
    uint8_t seg;
    size_t koff = 0, soff;

    struct mig_radix_node *cur, **curp = root;
    cur = *curp;

    while(cur != NULL && koff < klen)
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
    return curp;
}

struct mig_radix_tree *mig_radix_tree_create()
{
    struct mig_radix_tree *tree = malloc(sizeof(struct mig_radix_tree));
    tree->root = NULL;
    return tree;
}

void mig_radix_tree_insert(struct mig_radix_tree *tree, uint8_t *key, size_t klen, void *value)
{
    uint8_t seg;
    size_t koffset, soffset;
    struct mig_radix_node *new, **lmp = mig_radix_node_find(&tree->root, key, klen, &koffset, &soffset);

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

void *mig_radix_tree_lookup(struct mig_radix_tree *tree, uint8_t *key, size_t klen)
{
    struct mig_radix_node *lkn = *mig_radix_node_find(&tree->root, key, klen, NULL, NULL);
    return lkn ? lkn->value : NULL;
}

#define mig_radix_tree_insert_s(t, k, v) (mig_radix_tree_insert((t), (k), strlen(k), (v)))
#define mig_radix_tree_lookup_s(t, k) (mig_radix_tree_lookup((t), (k), strlen(k)))

int main()
{
    void *v;
    struct mig_radix_tree *t = mig_radix_tree_create();
    mig_radix_tree_insert_s(t, "cat", (void *) 0x10);
    mig_radix_tree_insert_s(t, "catastrophe", (void *) 0x20);
    mig_radix_tree_insert_s(t, "cape", (void *) 0x30);
    mig_radix_tree_insert_s(t, "capricorn", (void *) 0x40);

    v = mig_radix_tree_lookup_s(t, "cat");
    v = mig_radix_tree_lookup_s(t, "cape");
    v = mig_radix_tree_lookup_s(t, "catastrophe");
    v = mig_radix_tree_lookup_s(t, "catastro");
}

