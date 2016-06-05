#ifndef MIG_RADIX_TREE_H
#define MIG_RADIX_TREE_H

#include <stdlib.h>
#include <stdint.h>

struct mig_radix_node;

struct mig_radix_node
{
    void *value;
    uint8_t *segment;
    size_t seglen;
    struct mig_radix_node *nodes[256];
};

struct mig_radix_tree
{
    struct mig_radix_node *root;
};

#endif
