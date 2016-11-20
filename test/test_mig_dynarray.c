#include "mig_dynarray.h"

int main()
{
    struct mig_dynarray *arr = mig_dynarray_create();
    void *ref;
    mig_dynarray_init(arr, 3, 16, 1, MIG_DYNARRAY_TIGHT_ALLOC);
    for(int i = 0; i < 64; i++)
    {
        mig_dynarray_pushref(arr, &ref);
        printf("%p\n", ref);
    }
    mig_dynarray_destroy(arr);
    return 0;
}
