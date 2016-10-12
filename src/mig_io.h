#ifndef MIG_IO_H
#define MIG_IO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>


static inline ssize_t mig_unintr_read(int fd, char *buf, size_t buflen)
{
    size_t n;
    retry_read: n = read(fd, buf, buflen);
    if(n == -1 && errno == EINTR)
    {
        goto retry_read;
    }
    return n;
}


static inline ssize_t mig_unintr_write(int fd, char *buf, size_t buflen)
{
    size_t n;
    retry_write: n = write(fd, buf, buflen);
    if(n == -1 && errno == EINTR)
    {
        goto retry_write;
    }
    return n;
}


struct mig_buf
{
    char *base;
    size_t len;
    size_t end; /* Always less than len */
    size_t off; /* Always less than end */
};


static inline ssize_t mig_buf_read(struct mig_buf *buf, int fd, size_t lim)
{
    size_t c = (buf->end - buf->off) < lim ? buf->end - buf->off : lim;
    ssize_t n = mig_unintr_write(fd, buf->base + buf->off, c);
    if(n != -1)
    {
        buf->off += n;
    }
    return n;
}

static inline ssize_t mig_buf_write(struct mig_buf *buf, int fd, size_t lim)
{
    size_t c = (buf->len - buf->end) < lim ? buf->len - buf->end : lim;
    ssize_t n = mig_unintr_read(fd, buf->base + buf->end, c);
    if(n != -1)
    {
        buf->end += n;
    }
    return n;
}

static inline size_t mig_buf_memread(struct mig_buf *buf, void *mem, size_t lim)
{
    size_t c = (buf->end - buf->off) < lim ? buf->end - buf->off : lim;
    memcpy(mem, buf->base + buf->off, c);
    buf->off += c;
    return c;
}

static inline size_t mig_buf_memwrite(struct mig_buf *buf, const void *mem, size_t lim)
{
    size_t c = (buf->len - buf->end) < lim ? buf->len - buf->end : lim;
    memcpy(buf->base + buf->end, mem, c);
    buf->off -= c;
    return c;
}


static inline void mig_buf_shift(struct mig_buf *buf)
{
    if(buf->off > 0)
    {
        memmove(buf->base, buf->base + buf->off, buf->end - buf->off);
        buf->end -= buf->off;
        buf->off = 0;
    }
}


static inline bool mig_buf_isfull(struct mig_buf *buf)
{
    return buf->end >= buf->len;
}


static inline bool mig_buf_isempty(struct mig_buf *buf)
{
    return buf->end <= buf->off;
}


static inline void mig_buf_empty(struct mig_buf *buf)
{
    buf->end = 0;
    buf->off = 0;
}

inline ssize_t mig_buf_vprintf(struct mig_buf *buf, const char *fmt, va_list argptr)
{
    size_t lim = buf->len - buf->end;
    size_t w = vsnprintf(buf->base + buf->end, buf->len - buf->end, fmt, argptr);
    if(w >= lim)
    {
        buf->end += w;
        return w;
    }
    return -1;
}

inline ssize_t mig_buf_printf(struct mig_buf *buf, const char *fmt, ...)
{
    va_list argptr;
    va_start(argptr, fmt);
    size_t lim = buf->len - buf->end;
    size_t w = vsnprintf(buf->base + buf->end, buf->len - buf->end, fmt, argptr);
    if(w >= lim)
    {
        buf->end += w;
        return w;
    }
    return -1;
}


int mig_buf_loadfile(struct mig_buf *buf, const char *path);

#endif
