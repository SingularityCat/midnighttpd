#include <stdio.h>
#include "mig_core.h"

void handlein(struct mig_loop *lp, size_t idx)
{
    printf("Read a byte from stdin: %c.\n", getchar());
}

int main()
{
    struct mig_loop *lp = mig_loop_create(5);
    mig_loop_register(lp, 0, handlein, NULL, MIG_COND_READ, NULL);

    mig_loop_exec(lp);
}
