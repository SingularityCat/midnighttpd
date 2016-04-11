#ifndef MIG_CORE
#define MIG_CORE

#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>

/* size_t stack */
struct mig_zstk {
    size_t *base;
    size_t top;
    size_t len;
};

#define mig_zstk_init(stk, z) ((stk.base = calloc(stk.len = z, sizeof(size_t))), (stk.top = 0))
#define mig_zstk_push(stk, v) (stk.top < stk.len ? (stk.base[stk.top++] = v), true : false)
#define mig_zstk_pop(stk, v_ptr) (stk.top > 0 ? (*v_ptr = stk.base[--stk.top]), true : false)
#define mig_zstk_free(stk) (free(stk.base))

enum mig_cond {
    MIG_COND_READ  = POLLIN,
    MIG_COND_WRITE = POLLOUT,
    MIG_COND_ERR = POLLERR
};

struct mig_loop;
struct mig_ent;

typedef void (*mig_callback)(struct mig_loop *loop, size_t idx);

struct mig_loop {
    struct pollfd *entfds;
    struct mig_ent *entarr;
    struct mig_zstk freestk;
    size_t entlen;
};

struct mig_ent {
    mig_callback call;
    mig_callback free;
    void *data;
    int fd;
};

extern const struct mig_ent MIG_ENT_EMPTY;

struct mig_loop *mig_loop_create(size_t maxfds);
void mig_loop_destroy(struct mig_loop *loop);

size_t mig_loop_register(struct mig_loop *loop, int fd, mig_callback callfp, mig_callback freefp, enum mig_cond cond, void *dptr);
void mig_loop_unregister(struct mig_loop *loop, size_t idx);

void mig_loop_exec(struct mig_loop *loop);


inline void mig_loop_disable(struct mig_loop *loop, size_t idx)
{
    loop->entfds[idx].fd = -1;
}

inline void mig_loop_enable(struct mig_loop *loop, size_t idx)
{
    loop->entfds[idx].fd = loop->entarr[idx].fd;
}

inline void mig_loop_setcond(struct mig_loop *loop, size_t idx, enum mig_cond cond)
{
    loop->entfds[idx].events = cond;
}

inline enum mig_cond mig_loop_getcond(struct mig_loop *loop, size_t idx)
{
    return loop->entfds[idx].events;
}

inline enum mig_cond mig_loop_getactv(struct mig_loop *loop, size_t idx)
{
    return loop->entfds[idx].revents;
}

inline int mig_loop_getfd(struct mig_loop *loop, size_t idx)
{
    return loop->entarr[idx].fd;
}

inline void mig_loop_setfd(struct mig_loop *loop, size_t idx, int fd)
{
    loop->entarr[idx].fd = fd;
    if(!loop->entfds[idx].fd > 0)
    {
        loop->entfds[idx].fd = fd;
    }
}

inline void mig_loop_setcall(struct mig_loop *loop, size_t idx, mig_callback fp)
{
    loop->entarr[idx].call = fp;
}

inline void mig_loop_setfree(struct mig_loop *loop, size_t idx, mig_callback fp)
{
    loop->entarr[idx].free = fp;
}

#endif

