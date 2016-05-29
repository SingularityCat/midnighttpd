
#include <stdlib.h>
#include "mig_opt.h"

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
        opt = -1;
    }

    *argnp = offset;

    return opt;
}
