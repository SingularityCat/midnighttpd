#ifndef MIG_CORE_H
#define MIG_CORE_H

/* === midnight event loop ===
 *
 * The midnight event loop supports two different kinds of event handler,
 * a single callback and a call chain.
 *
 * Single callbacks should be a 'void function(mig_loop *, size_t)' (mig_callback).
 * These are called with a reference to the loop, and the index of the entry
 * associated with the event.
 *
 * Call chains should be an array of 'bool function(mig_loop *, size_t)' (mig_chainfunc).
 * The functions in this array are called with the same arguments as single callbacks.
 * The return vaule is used to determine wether or not to proceed to the next function in the chain.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <poll.h>

/* size_t stack */
struct mig_zstk {
    size_t *base;
    size_t top;
    size_t len;
};

#define mig_zstk_init(stk, z) (((stk).base = calloc((stk).len = z, sizeof(size_t))), ((stk).top = 0))
#define mig_zstk_push(stk, v) ((stk).top < (stk).len ? ((stk).base[(stk).top++] = v), true : false)
#define mig_zstk_pop(stk, v_ptr) ((stk).top > 0 ? (*v_ptr = (stk).base[--(stk).top]), true : false)
#define mig_zstk_free(stk) ((stk).top = 0, (stk).len = 0, free((stk).base))
#define mig_zstk_isfull(stk) ((stk).top == (stk).len)

enum mig_cond {
    MIG_COND_READ  = POLLIN,
    MIG_COND_WRITE = POLLOUT,
    MIG_COND_ERR = POLLERR
};

struct mig_loop;
struct mig_ent;

typedef void (*mig_callback)(struct mig_loop *loop, size_t idx);
typedef bool (*mig_chainfunc)(struct mig_loop *loop, size_t idx);

struct mig_loop {
    struct pollfd *entfds;
    struct mig_ent *entarr;
    struct mig_zstk freestk;
    size_t entlen;
    bool terminate;
};

enum mig_calltype {
    MIG_CALLTYPE_NULL,
    MIG_CALLTYPE_CALLBACK,
    MIG_CALLTYPE_CALLCHAIN
};

struct mig_ent {
    void *data;
    mig_callback free;
    union {
        mig_callback call;
        const mig_chainfunc *callchain;
    };
    enum mig_calltype calltype;
    int fd;
};

const extern mig_chainfunc MIG_CALLCHAIN_SENTINEL;

struct mig_loop *mig_loop_create(size_t maxfds);
void mig_loop_destroy(struct mig_loop *loop);

size_t mig_loop_register(struct mig_loop *loop, int fd, mig_callback callfp, mig_callback freefp, enum mig_cond cond, void *dptr);
size_t mig_loop_register_chain(struct mig_loop *loop, int fd, const mig_chainfunc *chain, mig_callback freefp, enum mig_cond cond, void *dptr);
void mig_loop_unregister(struct mig_loop *loop, size_t idx);

int mig_loop_exec(struct mig_loop *loop);
void mig_loop_terminate(struct mig_loop *loop);

void mig_loop_disable(struct mig_loop *loop, size_t idx);
void mig_loop_enable(struct mig_loop *loop, size_t idx);
void mig_loop_setcond(struct mig_loop *loop, size_t idx, enum mig_cond cond);
enum mig_cond mig_loop_getcond(struct mig_loop *loop, size_t idx);
enum mig_cond mig_loop_getactv(struct mig_loop *loop, size_t idx);
int mig_loop_getfd(struct mig_loop *loop, size_t idx);
void mig_loop_setfd(struct mig_loop *loop, size_t idx, int fd);
void *mig_loop_getdata(struct mig_loop *loop, size_t idx);
void mig_loop_setdata(struct mig_loop *loop, size_t idx, void *data);
void mig_loop_setfree(struct mig_loop *loop, size_t idx, mig_callback fp);
void mig_loop_setcall(struct mig_loop *loop, size_t idx, mig_callback fp);
void mig_loop_setcallchain(struct mig_loop *loop, size_t idx, const mig_chainfunc *chain);

#endif

