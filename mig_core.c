#include "mig_core.h"

struct mig_loop_ctx *mig_loop_create(size_t maxfds)
{
    size_t i;
    struct mig_loop_ctx *loop = NULL;
    loop = malloc(sizeof(struct mig_loop_ctx));
    loop->entlen = maxfds;
    loop->entfds = calloc(maxfds, sizeof(struct pollfd));
    loop->entarr = calloc(maxfds, sizeof(struct mig_ent_ctx));
    mig_zstk_init(loop->freestk, maxfds);
    for(i = maxfds - 1; i >= 0; i--)
    {
        loop->entfds[i].fd = -1;
        mig_zstk_push(loop->freestk, i);
    }
    return loop;
}

void mig_loop_destroy(struct mig_loop_ctx *loop)
{
    size_t i;
    struct mig_ent_ctx *entctxp;
    for(i = 0; i < loop->entlen; i++)
    {
        entctxp = loop->entarr + i;
        if(entctxp->free)
        {
            entctxp->free(loop, entctxp, i);
        }
    }

    mig_zstk_free(loop->freestk);
    free(loop->entarr);
    free(loop->entfds);
    free(loop);
}

size_t mig_loop_register(struct mig_loop_ctx *loop, int fd, struct mig_ent_ctx initctx)
{
    size_t arridx = -1;
    if(mig_zstk_pop(loop->freestk, &arridx))
    {
        loop->entarr[arridx] = initctx;
        loop->entfds[arridx].fd = initctx.fd;
        loop->entfds[arridx].events = initctx.cond;
    }
    return arridx;
}

void mig_loop_unregister(struct mig_loop_ctx *loop, size_t idx)
{
    struct mig_ent_ctx *entctxp = loop->entarr + idx;
    if(entctxp->fd != -1)
    {
        if(entctxp->free)
        {
            entctxp->free(loop, entctxp, idx);
        }
        *entctxp = empty_ctx;
        loop->entfds[idx].fd = -1;
        loop->entfds[idx].events = 0;
        loop->entfds[idx].revents = 0;
        mig_zstk_push(loop->freestk, idx);
    }
}

noreturn void mig_loop_exec(struct mig_loop_ctx *loop)
{
    int hfds, rfds;
    size_t idx;
    struct pollfd *curfdp;
    struct mig_ent_ctx *entctxp;
    while(1)
    {
        rfds = poll(loop->entfds, loop->entlen, -1);
        for(hfds = 0, idx = 0; idx < loop->entlen && hfds < rfds; idx++)
        {
            curfdp = fds + idx;
            if(curfdp->revents == 0) continue;
            entctxp = loop->entarr + curfdp->fd;
            if(entctxp->cond | curfdp->revents > 0)
            {
                entctxp->call(loop, entctxp, idx);
            }
            hfds++;
        }
    }
}
