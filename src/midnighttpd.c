#define _GNU_SOURCE
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>

#include "mig_core.h"
#include "mig_io.h"
#include "mhttp_util.h"
#include "mhttp_method.h"
#include "mhttp_range.h"
#include "mhttp_status.h"

#include "midnighttpd_config.h"

struct mhttp_req
{
    struct mig_buf rxbuf;
    struct mig_buf txbuf;
    enum mhttp_method method;
    const char *uri;
    const char *arg;
    struct mhttp_range range;
    bool eos;
    int srcfd;
    size_t srclen;
};

void mhttp_req_free(struct mig_loop *lp, size_t idx);
void mhttp_req_resetctx(struct mhttp_req *rctx);
void mhttp_req_init(struct mig_loop *lp, size_t idx);
void mhttp_req_recv(struct mig_loop *lp, size_t idx);
void mhttp_req_intr(struct mig_loop *lp, size_t idx);
void mhttp_req_send(struct mig_loop *lp, size_t idx);
void mhttp_send_dirindex(int fd, const char *dir);

void mhttp_accept(struct mig_loop *lp, size_t idx)
{
    int sock = accept(mig_loop_getfd(lp, idx), NULL, NULL);
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
    size_t ni = mig_loop_register(lp, sock, mhttp_req_init, mhttp_req_free, MIG_COND_READ, NULL);
    if(ni == (size_t) -1)
    {
        mhttp_send_error(sock, http503);
        close(sock);
        printf("[%zu] Rejecting new connection (no space left).\n", idx);
        return;
    }
    printf("[%zu] Accepting new connection as %zu\n", idx, ni);
}

void mhttp_req_free(struct mig_loop *lp, size_t idx)
{
    struct mhttp_req *ctx = mig_loop_getdata(lp, idx);
    if(ctx != NULL)
    {
        if(ctx->srcfd != -1) { close(ctx->srcfd); }
        free(ctx);
    }
    close(mig_loop_getfd(lp, idx));
    printf("[%zu] Connection closed.\n", idx);
}

void mhttp_req_resetctx(struct mhttp_req *rctx)
{
    rctx->range.low = 0;
    rctx->range.high = -1;
    rctx->range.spec = MHTTP_RANGE_SPEC_NONE;
    rctx->eos = false;
    rctx->srcfd = -1;
    rctx->srclen = 0;
}

void mhttp_req_init(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = malloc(
        sizeof(*rctx) +
        config.header_buflen +
        config.header_buflen);
    rctx->rxbuf.base = (char *) (rctx + 1);
    rctx->rxbuf.len = config.header_buflen;
    rctx->txbuf.base = rctx->rxbuf.base + rctx->rxbuf.len;
    rctx->txbuf.len = config.header_buflen;
    mig_buf_empty(&rctx->rxbuf);
    mhttp_req_resetctx(rctx);
    mig_loop_setdata(lp, idx, rctx);
    mig_loop_setcall(lp, idx, mhttp_req_recv);
    mhttp_req_recv(lp, idx);
}


void mhttp_req_recv(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    char *chkptr;
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    size_t prevend = rctx->rxbuf.end;
    size_t offset;

    if(mig_buf_full(&rctx->rxbuf))
    {
        /* Header too big. This is fatal. */
        mhttp_send_error(fd, http431);
        mig_loop_unregister(lp, idx);
        return;
    }

    size_t recvd = mig_buf_read(&rctx->rxbuf, fd, -1);
    if(recvd == -1)
    {
        printf("[%zu] recv error: %s\n", idx, strerror(errno));
        mig_loop_unregister(lp, idx);
        return;
    }

    printf("[%zu] recv'd %zu bytes of data.\n", idx, recvd);
    if(recvd == 0)
    {
        printf("[%zu] EOS before headers recv'd.\n", idx);
        mig_loop_unregister(lp, idx);
        return;
    }

    /* As we can recv bytes individually, we must check up to the previous 3 bytes as well */
    offset = prevend < 3 ? prevend : 3;
    chkptr = memmem(rctx->rxbuf.base + prevend - offset, recvd + offset, "\r\n\r\n", 4);

    if(chkptr != NULL)
    {
        printf("[%zu] Headers complete.\n", idx);
        /* Make a null terminated string */
        *chkptr = 0;
        rctx->rxbuf.end = chkptr - rctx->rxbuf.base;
        /* switch to intr */
        mig_loop_setcall(lp, idx, mhttp_req_intr);
        mig_loop_setcond(lp, idx, MIG_COND_WRITE);
    }
}

void mhttp_req_intr(struct mig_loop *lp, size_t idx)
{
    int srcfd, fd = mig_loop_getfd(lp, idx);
    int ret;
    char *chkptr;
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    size_t bufidx, sent;

    struct stat srcstat;

    /* Analyse client headers */
    rctx->method = mhttp_interpret_method(rctx->rxbuf.base);
    if (rctx->method == MHTTP_METHOD_NONE)
    {
        mhttp_send_error(fd, http501);
        mig_loop_unregister(lp, idx);
        return;
    }
    else if (rctx->method & MHTTP_METHOD_UNSUPPORTED)
    {
        dprintf(fd,
            "HTTP/1.1 %s\r\n"
            SERVER_HEADER
            "Content-Length: 0\r\n"
            "Allow: %s\r\n"
            "\r\n",
            http405,
            allowed_methods);
        mig_loop_unregister(lp, idx);
        return;
    }

    rctx->uri = strchr(rctx->rxbuf.base, ' ') + 1;
    chkptr = strchr(rctx->uri, ' ');
    if(chkptr == NULL && (chkptr - rctx->rxbuf.base) >= rctx->rxbuf.end)
    {
        goto malformed_err;
    }
    *chkptr = 0;
    mhttp_urldecode(rctx->uri, (char *) rctx->uri, chkptr+1 - rctx->uri);
    printf("[%zu] URI: %s\n", idx, rctx->uri);

    bufidx = (chkptr + 1) - rctx->rxbuf.base;

    chkptr = strstr(rctx->rxbuf.base + bufidx, "\r\n");
    while(chkptr != NULL)
    {
        chkptr += 2; /* Skip crlf */
        if(strncmp("Connection:", chkptr, 11) == 0)
        {
            chkptr += 11;
            while(*chkptr++ == ' '); /* Skip whitespace */
            rctx->eos = (strncmp("close", chkptr, 5) == 0);
        }
        else if(strncmp("Range:", chkptr, 6) == 0)
        {
            chkptr += 6;
            if(mhttp_parse_range(chkptr, &rctx->range)) { goto malformed_err; }
        }
        bufidx = (chkptr - rctx->rxbuf.base);
        chkptr = strstr(rctx->rxbuf.base + bufidx, "\r\n");
    }

    switch(rctx->method)
    {
        case MHTTP_METHOD_GET:
        case MHTTP_METHOD_HEAD:
            retry_stat:
            if(stat(rctx->uri+1, &srcstat))
            {
                switch(errno)
                {
                    case EINTR:
                        goto retry_stat;
                    case ENAMETOOLONG:
                        mhttp_send_error(fd, http414);
                        break;
                    case EACCES:
                        mhttp_send_error(fd, http403);
                        break;
                    case ENOENT:
                        mhttp_send_error(fd, http404);
                        break;
                    case ELOOP:
                        mhttp_send_error(fd, http508);
                        break;
                    default:
                        mhttp_send_error(fd, http500);
                }
                printf("[%zu] stat error: %s\n", idx, strerror(errno));
                goto keepalive;
            }

            /* Test if we're a directory. */
            if(S_ISDIR(srcstat.st_mode))
            {
                /* Should we do directory listing? */
                if(1)
                {
                    mhttp_send_dirindex(fd, rctx->uri);
                }
                else
                {
                    mhttp_send_error(fd, http403);
                }
                goto keepalive;
            }

            /* Range checking logic:
             * Invalid range -> ignored.
             * Lower bound higher then resource limit.
             */
            if(rctx->range.low > rctx->range.high)
            {
                rctx->range.spec = MHTTP_RANGE_SPEC_NONE;
            }
            if(rctx->range.low >= srcstat.st_size)
            {
                dprintf(fd,
                    "HTTP/1.1 " http416 "\r\n"
                    SERVER_HEADER
                    "Content-Length: 0\r\n"
                    "Content-Range: */%zu\r\n"
                    "\r\n",
                    (size_t) srcstat.st_size);
                goto keepalive;
            }
            if(rctx->range.high >= srcstat.st_size)
            {
                rctx->range.high = srcstat.st_size;
            }

            if(rctx->range.spec != MHTTP_RANGE_SPEC_NONE)
            {
                rctx->srclen = rctx->range.high - rctx->range.low;
                dprintf(fd,
                    "HTTP/1.1 " http206 "\r\n"
                    SERVER_HEADER
                    "Content-Length: %zu\r\n"
                    "Content-Range: bytes %zu-%zu/%zu\r\n"
                    "\r\n",
                    rctx->srclen,
                    rctx->range.low, rctx->range.high, (size_t) srcstat.st_size);
            }
            else
            {
                rctx->srclen = srcstat.st_size;
                dprintf(fd,
                    "HTTP/1.1 " http200 "\r\n"
                    SERVER_HEADER
                    "Content-Length: %zu\r\n"
                    "\r\n",
                    srcstat.st_size);
            }
            if(rctx->method == MHTTP_METHOD_GET)
            {
                rctx->srcfd = srcfd = open(rctx->uri+1, 0);
                if(rctx->range.spec != MHTTP_RANGE_SPEC_NONE)
                {
                    lseek(srcfd, rctx->range.low, SEEK_SET);
                }
                mig_loop_setcall(lp, idx, mhttp_req_send);
            }
            else
            {
                goto keepalive;
            }
            break;
        case MHTTP_METHOD_OPTIONS:
            dprintf(fd,
                "HTTP/1.1 %s\r\n"
                SERVER_HEADER
                "Allow: %s\r\n"
                "\r\n",
                http204,
                allowed_methods);
            if(!rctx->eos)
            {
                goto keepalive;
            }
            else
            {
                mig_loop_unregister(lp, idx);
            }
            break;
        default:
            goto malformed_err;
    }

    return;

    malformed_err:
    mhttp_send_error(fd, http400);
    mig_loop_unregister(lp, idx);
    return;

    keepalive:
    mhttp_req_resetctx(rctx);
    mig_loop_setcall(lp, idx, mhttp_req_recv);
    mig_loop_setcond(lp, idx, MIG_COND_READ);
    return;
}

void mhttp_req_send(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    size_t chunklen;
    ssize_t sent;

    /* Check if buffer is empty */
    if(rctx->txbuf.end == 0)
    {
        chunklen = rctx->srclen < rctx->txbuf.len ? rctx->srclen : rctx->txbuf.len;
        sent = mig_buf_read(&rctx->txbuf, rctx->srcfd, rctx->srclen); /* Sent here means amount read. */
        if(sent == -1) { goto io_error; }
        else if(sent == 0 && rctx->srclen > 0)
        {
            /* Huh? Early end of file? Set the EOS flag to bail gracefully */
            rctx->eos = true;
        }
        rctx->srclen -= sent;
    }

    sent = mig_buf_write(&rctx->txbuf, fd, -1);
    if(sent == -1) { goto io_error; }

    if(!rctx->eos)
    {
        if(rctx->srclen == 0)
        {
            close(rctx->srcfd);
            mhttp_req_resetctx(rctx);
            mig_buf_shift(&rctx->rxbuf);
            mig_buf_empty(&rctx->txbuf);
            mig_loop_setcall(lp, idx, mhttp_req_recv);
            mig_loop_setcond(lp, idx, MIG_COND_READ);
        }
        return;
    }

    /* Reached if there's an IO error or this is the end of the stream.
     * Note the mhttp_req_free function will close srcfd.
     */
    io_error:
    mig_loop_unregister(lp, idx);
}

void mhttp_send_dirindex(int fd, const char *dir)
{
    const size_t buflen = config.dirlst_buflen;
    char buf[buflen];
    size_t written;
    DIR *dirp;
    struct dirent *dent;

    const char * const preamble =
        "<!DOCTYPE html5>"
        "<head><title>%s</title></head>"
        "<body><h1>Directroy listing for %s</h1>"
            "<ul>";
    const char * const postamble =
            "</ul>"
        "<body>";

    dirp = opendir(dir+1);
    if(dirp == NULL) { goto dirlst_err; }
    written = snprintf(buf, buflen, preamble, dir, dir);

    while((dent = readdir(dirp)) != NULL)
    {
        written += snprintf(buf + written, buflen - written,
            "<li><a href=\"%s/%s\">%s</a></li>", dir, dent->d_name, dent->d_name);
    }

    written += snprintf(buf + written, buflen - written, postamble);

    dprintf(fd,
        "HTTP/1.1 " http200 "\r\n"
        SERVER_HEADER
        "Content-Length: %zu\r\n"
        "\r\n", written);
    send(fd, buf, written, 0);
    return;

    dirlst_err:
    mhttp_send_error(fd, http500);
}

int main(int argc, char **argv)
{
    int servsock;
    int ssopt_v = 1;
    struct sockaddr_in addr;

    memset((void *) &addr, 0, sizeof(addr));
    servsock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &ssopt_v, sizeof(ssopt_v));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(31337);
    if(bind(servsock, &addr, sizeof(addr)))
    {
        perror("bind");
        return 1;
    }
    listen(servsock, config.loop_slots);

    struct mig_loop *lp = mig_loop_create(config.loop_slots);
    mig_loop_register(lp, servsock, mhttp_accept, NULL, MIG_COND_READ, NULL);
    if(mig_loop_exec(lp))
    {
        perror("midnighttpd - loop failed");
        return -1;
    }
    return 0;
}
