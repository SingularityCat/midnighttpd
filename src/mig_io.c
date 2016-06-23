#include "mig_io.h"

#include <fcntl.h>
#include <sys/stat.h>

int mig_buf_loadfile(struct mig_buf *buf, const char *path)
{
    int fd;
    struct stat st;
    size_t len;
    ssize_t r;

    retry_open: fd = open(path, O_RDONLY);
    if(fd == -1)
    {
        if(errno == EINTR) { goto retry_open; }
        else { return -1; }
    }

    retry_fstat:
    if(fstat(fd, &st))
    {
        if(errno == EINTR) { goto retry_fstat; }
        else { close(fd); return -1; }
    }

    len = st.st_size;
    if(buf->base != NULL)
    {
        len = (buf->len - buf->end) < len ? (buf->len - buf->end) : len;
    }
    else
    {
        buf->base = malloc(len);
        buf->len = len;
        buf->end = 0;
        buf->off = 0;
    }

    r = mig_unintr_read(fd, buf->base + buf->end, len);
    close(fd);

    if(r == -1)
    {
        return -1;
    }

    buf->end += r;
    return 0;
}
