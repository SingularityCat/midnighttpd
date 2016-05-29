#include <stdlib.h>
#include "mig_opt.h"

/* mig_opt - getopt replacement functions. */

struct mig_optcfg
{
    struct
    {
        int mandatory;
        int optional;
    } opts[128];
};

struct mig_optcfg *mig_optcfg_create()
{
    struct mig_optcfg *o = calloc(sizeof(struct mig_optcfg), 1);
    return o;
}

void mig_optcfg_destroy(struct mig_optcfg *o)
{
    free(o);
}

void mig_setopt(struct mig_optcfg *o, char opt, int m_cnt, int o_cnt)
{
    o->opts[opt].mandatory = m_cnt;
    o->opts[opt].optional = o_cnt;
}


/* mig_getopt - getopt replacement function.
 * Takes four arguments, a mig_optcfg, a pointer to the length of the argument list,
 * a pointer to the argument list, and a pointer to the number of entries to skip.
 *
 * Returns the option read.
 * On completion, the 3 pointer arguments are updated.
 *  - argcp is reduced by the number of argv entries traversed.
 *  - argvp is updated to point to the first argument after the option.
 *  - argnp will contain the number of arguments associated with the option.
 *
 * If the end of the array is reached, -1 is returned.
 * If the first parsed argument does not seem to be a option, 0 is returned.
 */

int mig_getopt(struct mig_optcfg *o, int *argcp, char ***argvp, int *argnp)
{
    if(*argcp <= *argnp)
    {
        return -1;
    }

    int offset = *argnp;
    char **argv = *argvp + offset;
    char *arg = *argv;
    int lim, opt;

    *argvp = argv + 1;
    *argcp -= offset + 1;
    offset = 0;
    opt = arg[0];

    if(opt == '-')
    {
        opt = arg[1];
        offset += o->opts[opt].mandatory;
        lim = offset + o->opts[opt].optional;
        lim = lim < *argcp ? lim : *argcp;
        for(argv++; offset < lim; offset++)
        {
            if(argv[offset][0] == '-')
            {
                break;
            }
        }
    }
    else
    {
        opt = 0;
    }

    *argnp = offset;

    return opt;
}
