#ifndef MIG_CORE
#define MIG_CORE

#include <stdlib.h>
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

enum mig_ent_cond;
struct mig_loop_ctx;
struct mig_ent_ctx;

typedef void (*mig_ent_callback)(struct mig_loop_ctx *, struct mig_ent_ctx *, size_t);

enum mig_ent_cond {
    MIG_ENT_READ  = POLLIN,
    MIG_ENT_WRITE = POLLOUT
};

struct mig_loop_ctx {
    struct pollfd *entfds;
    struct mig_ent_ctx *entarr;
    struct mig_zstk freestk;
    size_t entlen;
    int fdoffset;
};

struct mig_ent_ctx {
    mig_ent_callback call;
    mig_ent_callback free;
    void *data;
    int fd;
    enum mig_ent_cond cond;
} empty_ctx = {
    NULL,
    NULL,
    NULL,
    -1,
    0
};

struct mig_loop_ctx *mig_loop_create(size_t maxfds);
void mig_loop_destroy(struct mig_loop_ctx *loop);

size_t mig_loop_register(struct mig_loop_ctx *loop, int fd, struct mig_ent_ctx initctx);
void mig_loop_unregister(struct mig_loop_ctx *loop, int fd);

noreturn void mig_loop_exec(struct mig_loop_ctx *loop);

#endif

