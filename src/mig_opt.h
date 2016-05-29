#ifndef MIG_OPT_H
#define MIG_OPT_H

struct mig_optcfg;

struct mig_optcfg *mig_optcfg_create();
void mig_optcfg_destroy(struct mig_optcfg *o);

void mig_setopt(struct mig_optcfg *o, char opt, int m_cnt, int o_cnt);
int mig_getopt(struct mig_optcfg *o, int *argcp, char ***argvp, int *argnp);

#endif
