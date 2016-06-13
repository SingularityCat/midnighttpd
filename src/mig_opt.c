#include <stdlib.h>
#include "mig_opt.h"

/* mig_opt - getopt replacement functions. */

struct mig_optcfg
{
    struct
    {
        unsigned int mandatory;
        unsigned int optional;
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
    opt &= 0x7F;
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

    int argn = *argnp, offset, opt;
    unsigned int lim;
    char **argv = *argvp + argn;
    char *arg = *argv;

    opt = arg[0];

    if(opt != '-')
    {
        offset = 1;
        opt = 0;
    }
    else
    {
        argn++;
        opt = arg[1] & 0x7F;
        offset = o->opts[opt].mandatory;
        lim = o->opts[opt].optional;
        lim = lim > offset || lim == 0 ? lim : -1;
        lim = lim < *argcp ? lim : *argcp;
        for(argv++; offset < lim; offset++)
        {
            if(argv[offset][0] == '-')
            {
                break;
            }
        }
    }

    *argvp = argv;
    *argcp -= argn;
    *argnp = offset;

    return opt;
}
