#include <stdio.h>

#include "mig_opt.h"

int main(int argc, char **argv)
{
    struct mig_optcfg *o = mig_optcfg_create();
    mig_setopt(o, 'm', 1, 0);
    mig_setopt(o, 'o', 0, 1);
    mig_setopt(o, 'b', 1, 1);
    mig_setopt(o, 'n', 2, 1);
    mig_setopt(o, 'h', 0, 0);

    int argn = 0;
    argc--; argv++;

    int opt = mig_getopt(o, &argc, &argv, &argn);
    while(opt != -1)
    {
        printf("opt: %c, argn: %d\n", opt, argn);
        for(int i = 0; i < argn; i++)
        {
            printf("%s\n", argv[i]);
        }
        opt = mig_getopt(o, &argc, &argv, &argn);
    }

    mig_optcfg_destroy(o);
    return 0;
}
