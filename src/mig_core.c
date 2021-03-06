#include "mig_core.h"

static bool mig_callchain_sentinel(struct mig_loop *loop, size_t idx)
{
    mig_loop_unregister(loop, idx);
    return false;
}

const mig_chainfunc MIG_CALLCHAIN_SENTINEL = mig_callchain_sentinel;

static const struct mig_ent MIG_ENT_EMPTY = {
    NULL,
    NULL,
    NULL,
    -1
};

struct mig_loop *mig_loop_create(size_t maxfds)
{
    size_t i;
    struct mig_loop *loop = NULL;
    loop = malloc(sizeof(struct mig_loop));
    loop->entlen = maxfds;
    loop->entfds = calloc(maxfds, sizeof(struct pollfd));
    loop->entarr = calloc(maxfds, sizeof(struct mig_ent));
    mig_zstk_init(loop->freestk, maxfds);
    for(i = maxfds; i > 0; i--)
    {
        loop->entfds[i - 1].fd = -1;
        (void) mig_zstk_push(loop->freestk, i - 1);
    }
    loop->terminate = false;
    return loop;
}

void mig_loop_destroy(struct mig_loop *loop)
{
    size_t i;
    struct mig_ent *entctxp;
    for(i = 0; i < loop->entlen; i++)
    {
        entctxp = loop->entarr + i;
        if(entctxp->free)
        {
            entctxp->free(loop, i);
        }
    }

    mig_zstk_free(loop->freestk);
    free(loop->entarr);
    free(loop->entfds);
    free(loop);
}


size_t mig_loop_register(struct mig_loop *loop, int fd, mig_callback callfp, mig_callback freefp, enum mig_cond cond, void *dptr)
{
    size_t idx = -1;
    if(mig_zstk_pop(loop->freestk, &idx))
    {
        loop->entarr[idx].fd = fd;
        loop->entarr[idx].calltype = MIG_CALLTYPE_CALLBACK;
        loop->entarr[idx].call = callfp;
        loop->entarr[idx].free = freefp;
        loop->entarr[idx].data = dptr;
        loop->entfds[idx].fd = fd;
        loop->entfds[idx].events = cond;
        loop->entfds[idx].revents = 0;
    }
    return idx;
}

size_t mig_loop_register_chain(struct mig_loop *loop, int fd, const mig_chainfunc *chain, mig_callback freefp, enum mig_cond cond, void *dptr)
{
    size_t idx = -1;
    if(mig_zstk_pop(loop->freestk, &idx))
    {
        loop->entarr[idx].fd = fd;
        loop->entarr[idx].calltype = MIG_CALLTYPE_CALLCHAIN;
        loop->entarr[idx].callchain = chain;
        loop->entarr[idx].free = freefp;
        loop->entarr[idx].data = dptr;
        loop->entfds[idx].fd = fd;
        loop->entfds[idx].events = cond;
        loop->entfds[idx].revents = 0;
    }
    return idx;
}

void mig_loop_unregister(struct mig_loop *loop, size_t idx)
{
    struct mig_ent *entctxp = loop->entarr + idx;
    if(entctxp->fd != -1)
    {
        if(entctxp->free)
        {
            entctxp->free(loop, idx);
        }
        *entctxp = MIG_ENT_EMPTY;
        loop->entfds[idx].fd = -1;
        loop->entfds[idx].events = 0;
        loop->entfds[idx].revents = 0;
        (void) mig_zstk_push(loop->freestk, idx);
    }
}


int mig_loop_exec(struct mig_loop *loop)
{
    int hfds, rfds;
    size_t idx;
    struct pollfd *curfdp;
    struct mig_ent *curent;
    while(!loop->terminate)
    {
        if(mig_zstk_isfull(loop->freestk))
        {
            break;
        }
        rfds = poll(loop->entfds, loop->entlen, -1);
        if(rfds < 0)
        {
            if(errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        for(hfds = 0, idx = 0; idx < loop->entlen && hfds < rfds; idx++)
        {
            curfdp = loop->entfds + idx;
            if((curfdp->events & curfdp->revents) > 0)
            {
                hfds++;
                curent = loop->entarr + idx;
                switch(curent->calltype)
                {
                    case MIG_CALLTYPE_CALLBACK:
                        curent->call(loop, idx);
                        break;
                    case MIG_CALLTYPE_CALLCHAIN:
                        if((*curent->callchain)(loop, idx))
                        {
                            /* Move to next function only if we're still a callchain */
                            curent->calltype == MIG_CALLTYPE_CALLCHAIN && curent->callchain++;
                        }
                        break;
                }
            }
        }
    }
    return 0;
}

void mig_loop_terminate(struct mig_loop *loop)
{
    loop->terminate = true;
}

extern inline void mig_loop_disable(struct mig_loop *loop, size_t idx)
{
    loop->entfds[idx].fd = -1;
}

extern inline void mig_loop_enable(struct mig_loop *loop, size_t idx)
{
    loop->entfds[idx].fd = loop->entarr[idx].fd;
}

extern inline void mig_loop_setcond(struct mig_loop *loop, size_t idx, enum mig_cond cond)
{
    loop->entfds[idx].events = cond;
}

extern inline enum mig_cond mig_loop_getcond(struct mig_loop *loop, size_t idx)
{
    return loop->entfds[idx].events;
}

extern inline enum mig_cond mig_loop_getactv(struct mig_loop *loop, size_t idx)
{
    return loop->entfds[idx].revents;
}

extern inline int mig_loop_getfd(struct mig_loop *loop, size_t idx)
{
    return loop->entarr[idx].fd;
}

extern inline void mig_loop_setfd(struct mig_loop *loop, size_t idx, int fd)
{
    loop->entarr[idx].fd = fd;
    if(loop->entfds[idx].fd != -1)
    {
        loop->entfds[idx].fd = fd;
    }
}

extern inline void *mig_loop_getdata(struct mig_loop *loop, size_t idx)
{
    return loop->entarr[idx].data;
}

extern inline void mig_loop_setdata(struct mig_loop *loop, size_t idx, void *data)
{
    loop->entarr[idx].data = data;
}

extern inline void mig_loop_setfree(struct mig_loop *loop, size_t idx, mig_callback fp)
{
    loop->entarr[idx].free = fp;
}

extern inline void mig_loop_setcall(struct mig_loop *loop, size_t idx, mig_callback fp)
{
    if(fp)
    {
        loop->entarr[idx].calltype = MIG_CALLTYPE_CALLBACK;
        loop->entarr[idx].call = fp;
    }
    else
    {
        loop->entarr[idx].calltype = MIG_CALLTYPE_NULL;
    }
}

extern inline void mig_loop_setcallchain(struct mig_loop *loop, size_t idx, const mig_chainfunc *chain)
{
    if(chain)
    {
        loop->entarr[idx].calltype = MIG_CALLTYPE_CALLCHAIN;
        loop->entarr[idx].callchain = chain;
    }
    else
    {
        loop->entarr[idx].calltype = MIG_CALLTYPE_NULL;
    }
}

