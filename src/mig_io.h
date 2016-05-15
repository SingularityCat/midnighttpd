#ifndef MIG_IO_H
#define MIG_IO_H

#include <stdlib.h>
#include <unistd.h>
#include <string.h>


static inline size_t mig_unintr_read(int fd, char *buf, size_t buflen)
{
    size_t n;
    retry_read: n = read(fd, buf, buflen);
    if(n == -1 && errno == EINTR)
    {
        goto retry_read;
    }
    return n;
}


static inline size_t mig_unintr_write(int fd, char *buf, size_t buflen)
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


static inline size_t mig_buf_read(struct mig_buf *buf, int fd)
{
    size_t n = mig_unintr_read(fd, buf->base + buf->end, buf->len - buf->end);
    buf->end += n;
    return n;
}


static inline size_t mig_buf_write(struct mig_buf *buf, int fd)
{
    size_t n = mig_unintr_write(fd, buf->base + buf->off, buf->end - buf->off);
    buf->off += n;
    return n;
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


static inline void mig_buf_empty(struct mig_buf *buf)
{
    buf->end = 0;
    buf->off = 0;
}

#endif
