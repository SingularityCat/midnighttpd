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

struct mig_radix_node *mig_radix_node_create(void);
void mig_radix_node_destroy(struct mig_radix_node *node);
struct mig_radix_node **mig_radix_node_find(struct mig_radix_node **root, const uint8_t *key, size_t klen,
                                            size_t *koffp, size_t *soffp, struct mig_radix_node **prevp);

struct mig_radix_tree *mig_radix_tree_create(void);
void mig_radix_tree_destroy(struct mig_radix_tree *tree);
void mig_radix_tree_insert(struct mig_radix_tree *tree, const uint8_t *key, size_t klen, void *value);
void mig_radix_tree_remove(struct mig_radix_tree *tree, const uint8_t *key, size_t klen);
void *mig_radix_tree_lookup(struct mig_radix_tree *tree, const uint8_t *key, size_t klen);
void *mig_radix_tree_lpm(struct mig_radix_tree *tree, const uint8_t *key, size_t klen);

#endif
