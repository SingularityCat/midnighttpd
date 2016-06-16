#include "mig_radix_tree.h"

#define mig_radix_tree_insert_s(t, k, v) (mig_radix_tree_insert((t), (uint8_t *)(k), strlen(k), (v)))
#define mig_radix_tree_lookup_s(t, k) (mig_radix_tree_lookup((t), (uint8_t *)(k), strlen(k)))

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

    mig_radix_tree_destroy(t);
}

