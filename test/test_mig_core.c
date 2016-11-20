#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include <unistd.h>
#include "mig_core.h"

void noop(struct mig_loop *lp, size_t idx) {}
bool cft_stage1(struct mig_loop *lp, size_t idx);
bool cft_stage2(struct mig_loop *lp, size_t idx);
bool cft_stage3(struct mig_loop *lp, size_t idx);

int main(int argc, char **argv)
{
    int testin[2], testout[2];
    pipe(testin);
    pipe(testout);
    int r, s;

    if(fork())
    {
        r = testout[0];
        s = testin[1];
        close(testout[1]);
        close(testin[0]);

        char res[1];
        write(s, "abcdabcd", 8);
        read(r, res, 1);
        assert(res[0] == 42);
    }
    else
    {
        r = testin[0];
        s = testout[1];
        close(testin[1]);
        close(testout[0]);

        struct mig_loop *loop = mig_loop_create(8);
        mig_chainfunc funcs[] = {cft_stage1, cft_stage2, cft_stage3, MIG_CALLCHAIN_SENTINEL};
        mig_loop_register_chain(loop, r, funcs, noop, MIG_COND_READ, (void *) s);
        if(mig_loop_exec(loop))
        {
            perror("error");
        }
        else
        {
            puts("successful termination of loop");
        }
    }
}

static int s1rep = 3;
static int s2rep = 3;

bool cft_stage1(struct mig_loop *lp, size_t idx)
{
    char buf[1];
    read(mig_loop_getfd(lp, idx), buf, sizeof(buf));
    if(s1rep--)
    {
        return false;
    }
    puts("finished cft stage 1");
    return true;
}

bool cft_stage2(struct mig_loop *lp, size_t idx)
{
    char buf[1];
    read(mig_loop_getfd(lp, idx), buf, sizeof(buf));
    if(s2rep--)
    {
        return false;
    }
    puts("finished cft stage 2");
    mig_loop_setfd(lp, idx, (int) mig_loop_getdata(lp, idx));
    mig_loop_setcond(lp, idx, MIG_COND_WRITE);
    return true;
}

bool cft_stage3(struct mig_loop *lp, size_t idx)
{
    char buf[1] = {42};
    write(mig_loop_getfd(lp, idx), buf, sizeof(buf));
    mig_loop_unregister(lp, idx);
    puts("finished cft stage 3");
    return false;
}
