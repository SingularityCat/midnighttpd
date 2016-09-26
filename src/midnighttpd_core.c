#define _GNU_SOURCE
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <dirent.h>

#include "mig_core.h"
#include "mig_io.h"
#include "mhttp_req.h"
#include "mhttp_status.h"

#include "midnighttpd_config.h"

#include "midnighttpd_core.h"

void conn_accept(struct mig_loop *lp, size_t idx)
{
    int sock = accept(mig_loop_getfd(lp, idx), NULL, NULL);
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
    size_t ni = mig_loop_register(lp, sock, conn_init, conn_free, MIG_COND_READ, NULL);
    if(ni == (size_t) -1)
    {
        mhttp_send_error(sock, MHTTP_VERSION_1_0, http503);
        close(sock);
        debug("[%zu] Rejecting new connection (no space left).\n", idx);
        return;
    }
    debug("[%zu] Accepting new connection as %zu\n", idx, ni);
}


void conn_free(struct mig_loop *lp, size_t idx)
{
    struct mhttp_req *ctx = mig_loop_getdata(lp, idx);
    if(ctx != NULL)
    {
        mhttp_req_destroy(ctx);
    }
    close(mig_loop_getfd(lp, idx));
    debug("[%zu] Connection closed.\n", idx);
}


void conn_init(struct mig_loop *lp, size_t idx)
{
    struct mhttp_req *rctx = mhttp_req_create(
        config.rx_buflen,
        config.tx_buflen
    );
    mhttp_req_reset(rctx);
    mig_loop_setdata(lp, idx, rctx);
    mig_loop_setcall(lp, idx, conn_recv);
    conn_recv(lp, idx);
}


void conn_recv(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    size_t prevend = rctx->rxbuf.end;

    size_t recvd = mig_buf_read(&rctx->rxbuf, fd, -1);
    if(recvd == -1)
    {
        debug("[%zu] recv error: %s\n", idx, strerror(errno));
        rctx->eos = true;
        req_terminate(lp, idx, rctx);
        return;
    }

    debug("[%zu] recv'd %zu bytes of data.\n", idx, recvd);
    if(recvd == 0)
    {
        debug("[%zu] EOS before headers recv'd.\n", idx);
        rctx->eos = true;
        req_terminate(lp, idx, rctx);
        return;
    }

    if(mhttp_req_check(rctx, prevend))
    {
        debug("[%zu] Headers complete.\n", idx);
        /* switch to intr */
        mig_loop_setcall(lp, idx, conn_intr);
        mig_loop_setcond(lp, idx, MIG_COND_WRITE);
    }
    else if(mig_buf_isfull(&rctx->rxbuf))
    {
        /* Header too big. This is fatal. */
        mhttp_send_error(fd, rctx->version, http431);
        rctx->eos = true;
        req_terminate(lp, idx, rctx);
    }
}

void conn_intr(struct mig_loop *lp, size_t idx)
{
    int srcfd, fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    struct stat srcstat;

    if(!mhttp_req_parse(rctx))
    {
        goto malformed_err;
    }



    const char *mimetype = NULL;

    /* Find path extension */
    const char *ext = NULL, *ptr = rctx->path;
    while(*ptr)
    {
        switch(*ptr)
        {
            case '.':
                ext = ptr + 1;
                break;
            case '/':
                ext = NULL;
                break;
        }
        ptr++;
    }

    if(ext)
    {
        mimetype = mig_radix_tree_lookup(config.mimetypes, (const uint8_t *) ext, ptr - ext);
    }
    if(!mimetype)
    {
        mimetype = config.default_mimetype;
    }

    const char *allowed_methods = "GET,HEAD,OPTIONS";
    switch(rctx->method)
    {
        case MHTTP_METHOD_GET:
        case MHTTP_METHOD_HEAD:
            retry_stat:
            if(stat(rctx->path, &srcstat))
            {
                switch(errno)
                {
                    case EINTR:
                        goto retry_stat;
                    case ENAMETOOLONG:
                        mhttp_send_error(fd, rctx->version, http414);
                        break;
                    case EACCES:
                        mhttp_send_error(fd, rctx->version, http403);
                        break;
                    case ENOENT:
                        mhttp_send_error(fd, rctx->version, http404);
                        break;
                    case ELOOP:
                        mhttp_send_error(fd, rctx->version, http508);
                        break;
                    default:
                        mhttp_send_error(fd, rctx->version, http500);
                }
                debug("[%zu] stat error: %s\n", idx, strerror(errno));
                break;
            }

            /* Test if we're a directory. */
            if(S_ISDIR(srcstat.st_mode))
            {
                /* Should we do directory listing? */
                if(config.dirindex_enabled)
                {
                    conn_send_dirindex(fd, rctx);
                }
                else
                {
                    mhttp_send_error(fd, rctx->version, http403);
                }
                break;
            }

            /* Range checking logic:
             * Invalid range -> ignored.
             * Lower bound higher then resource limit -> 416.
             * Valid range -> 206.
             * Note: HTTP range requests are upper bound inclusive, i.e.
             *   Range: bytes=0-0
             * means the first byte.
             */
            if(rctx->range.low > rctx->range.high)
            {
                rctx->range.spec = MHTTP_RANGE_SPEC_NONE;
            }

            /* Turn inclusive bound into exclusive bound,
             * such that, high - low = length
             */
            rctx->range.high++;
            if(rctx->range.spec != MHTTP_RANGE_SPEC_NONE)
            {
                if(rctx->range.high > srcstat.st_size)
                {
                    rctx->range.high = srcstat.st_size;
                }
                if(rctx->range.low >= srcstat.st_size)
                {
                    dprintf(fd,
                        "%s " http416 "\r\n"
                        SERVER_HEADER
                        "Content-Length: 0\r\n"
                        "Content-Range: */%zu\r\n"
                        "Content-Type: %s\r\n"
                        "\r\n", mhttp_str_ver(rctx->version),
                        (size_t) srcstat.st_size,
                        mimetype);
                    break;
                }
                else
                {
                    /* Handle incomplete range specifications. */
                    if((rctx->range.spec & MHTTP_RANGE_SPEC_LOW) == 0)
                    {
                        rctx->range.low = srcstat.st_size - rctx->range.high;
                        rctx->range.high = srcstat.st_size;
                    }
                    else if((rctx->range.spec & MHTTP_RANGE_SPEC_HIGH) == 0)
                    {
                        rctx->range.high = srcstat.st_size;
                    }

                    rctx->srclen = rctx->range.high - rctx->range.low;
                    dprintf(fd,
                        "%s " http206 "\r\n"
                        SERVER_HEADER
                        "Content-Length: %zu\r\n"
                        "Content-Range: bytes %zu-%zu/%zu\r\n"
                        "Content-Type: %s\r\n"
                        "\r\n", mhttp_str_ver(rctx->version),
                        rctx->srclen,
                        /* range.high is exclusive, account for this. */
                        rctx->range.low, rctx->range.high - 1, (size_t) srcstat.st_size,
                        mimetype);
                    /* continue */
                }
            }
            else
            {
                rctx->srclen = srcstat.st_size;
                dprintf(fd,
                    "%s " http200 "\r\n"
                    SERVER_HEADER
                    "Content-Length: %zu\r\n"
                    "Content-Type: %s\r\n"
                    "\r\n", mhttp_str_ver(rctx->version),
                    (size_t) srcstat.st_size,
                    mimetype);
                /* continue */
            }
            if(rctx->method == MHTTP_METHOD_GET)
            {
                rctx->srcfd = srcfd = open(rctx->path, 0);
                if(rctx->range.spec != MHTTP_RANGE_SPEC_NONE)
                {
                    lseek(srcfd, rctx->range.low, SEEK_SET);
                }
                mig_loop_setcall(lp, idx, conn_send_file);
                return;
            }
            break;
        case MHTTP_METHOD_OPTIONS:
            dprintf(fd,
                "%s " http204 "\r\n"
                SERVER_HEADER
                "Allow: %s\r\n"
                "\r\n", mhttp_str_ver(rctx->version),
                allowed_methods);
            break;
        case MHTTP_METHOD_PUT:
        case MHTTP_METHOD_PATCH:
        case MHTTP_METHOD_DELETE:
            dprintf(fd,
                "%s " http405 "\r\n"
                SERVER_HEADER
                "Content-Length: 0\r\n"
                "Allow: %s\r\n"
                "\r\n", mhttp_str_ver(rctx->version),
                allowed_methods);
            break;
        default:
            mhttp_send_error(fd, rctx->version, http501);
            rctx->eos = true;
            break;
    }
    goto terminate;

    malformed_err:
    mhttp_send_error(fd, rctx->version, http400);
    rctx->eos = true;
    terminate:
    req_terminate(lp, idx, rctx);
    return;
}

void conn_send_file(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    ssize_t sent;

    /* Check if buffer is empty */
    if(mig_buf_isempty(&rctx->txbuf))
    {
        mig_buf_empty(&rctx->txbuf);
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

    if(rctx->srclen == 0)
    {
        close(rctx->srcfd);
        req_terminate(lp, idx, rctx);
    }
    return;

    /* Reached if there's an IO error. mhttp_req_free function will close srcfd. */
    io_error:
    rctx->eos = true;
    req_terminate(lp, idx, rctx);
}


void req_terminate(struct mig_loop *lp, size_t idx, struct mhttp_req *req)
{
    printf("[%zu] %s %s %s (%s)\n", idx, mhttp_str_ver(req->version), mhttp_str_method(req->method), req->path, req->args);

    if(req->eos)
    {
        /* close */
        mig_loop_unregister(lp, idx);
        return;
    }

    /* keepalive */
    mhttp_req_reset(req);
    mig_buf_shift(&req->rxbuf);
    mig_buf_empty(&req->txbuf);
    /* Check if there's a complete request queued up. */
    if(mhttp_req_check(req, 0))
    {
        mig_loop_setcall(lp, idx, conn_intr);
        mig_loop_setcond(lp, idx, MIG_COND_WRITE);

    }
    else
    {
        mig_loop_setcall(lp, idx, conn_recv);
        mig_loop_setcond(lp, idx, MIG_COND_READ);
    }
}


void conn_send_dirindex(int fd, struct mhttp_req *rctx)
{
    const size_t buflen = config.dirindex_buflen;
    char buf[buflen];
    size_t written;
    const char *dir = rctx->path;
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

    dirp = opendir(dir);
    if(dirp == NULL) { goto dirindex_err; }
    written = snprintf(buf, buflen, preamble, dir, dir);

    while((dent = readdir(dirp)) != NULL)
    {
        written += snprintf(buf + written, buflen - written,
            "<li><a href=\"%s/%s\">%s</a></li>", dir, dent->d_name, dent->d_name);
    }

    written += snprintf(buf + written, buflen - written, postamble);
    closedir(dirp);

    dprintf(fd,
        "%s " http200 "\r\n"
        SERVER_HEADER
        "Content-Length: %zu\r\n"
        "Content-Type: text/html\r\n"
        "\r\n", mhttp_str_ver(rctx->version), written);
    send(fd, buf, written, 0);
    return;

    dirindex_err:
    mhttp_send_error(fd, rctx->version, http500);
}

void conn_close_listen_sock(struct mig_loop *lp, size_t idx)
{
    close(mig_loop_getfd(lp, idx));
}

void conn_close_listen_sockunix(struct mig_loop *lp, size_t idx)
{
    char *path = mig_loop_getdata(lp, idx);
    close(mig_loop_getfd(lp, idx));
    if(path != NULL)
    {
        unlink(path);
        free(path);
    }
}

