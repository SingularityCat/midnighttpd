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
#include "mhttp_util.h"
#include "mhttp_method.h"
#include "mhttp_range.h"

#define MIDNIGHTTPD "midnighttpd"

#define MAX_CONNS 5

#ifndef SECRECY
    #define SERVER_HEADER "Server: " MIDNIGHTTPD "\r\n"
#else
    #define SERVER_HEADER ""
#endif

#define http200 "200 OK"
#define http204 "204 No content"
#define http206 "206 Partial Content"
#define http400 "400 Malformed Reqeuest"
#define http403 "403 Forbidden"
#define http404 "404 Not Found"
#define http405 "405 Method Not Allowed"
#define http414 "414 Request-URI Too Long"
#define http416 "416 Requested Range Not Satisfiable"
#define http431 "431 Request Header Fields Too Large"
#define http500 "500 Internal Server Error"
#define http501 "501 Not Implemented"
#define http503 "503 Service Unavailable"
#define http508 "508 Loop Detected"

#define mhttp_error_resp(error, ...) \
    ("HTTP/1.1 " error "\r\n"\
     SERVER_HEADER\
     "Content-Length: 0\r\n"\
     __VA_ARGS__\
     "\r\n")

#define mhttp_send_error(fd, error, ...) \
    send(fd, mhttp_error_resp(error, __VA_ARGS__), sizeof(mhttp_error_resp(error, __VA_ARGS__)), 0)

#define BUF_LEN 2048

struct mhttp_req
{
    char *buf;
    size_t buflen;
    size_t bufend;
    size_t bufoff; /* Used in req_send */
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
}

void mhttp_req_resetctx(struct mhttp_req *rctx)
{
    rctx->bufend = 0;
    rctx->bufoff = 0;
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
    struct mhttp_req *rctx = malloc(sizeof(*rctx) + BUF_LEN);
    rctx->buf = (char *) (rctx + 1);
    rctx->buflen = BUF_LEN;
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
    size_t prevend = rctx->bufend;
    size_t offset;

    if(rctx->bufend >= rctx->buflen)
    {
        /* Header too big. This is fatal. */
        mhttp_send_error(fd, http431);
        mig_loop_unregister(lp, idx);
        return;
    }

    size_t recvd = recv(fd, rctx->buf + rctx->bufend, rctx->buflen - rctx->bufend, 0);
    if(recvd == -1)
    {
        printf("[%zu] recv error: %s\n", idx, strerror(errno));
        mig_loop_unregister(lp, idx);
        return;
    }

    rctx->bufend += recvd;
    printf("[%zu] recv'd %zu bytes of data.\n", idx, recvd);
    if(recvd == 0)
    {
        printf("[%zu] Connection closed.\n", idx);
        mig_loop_unregister(lp, idx);
        return;
    }

    /* As we can recv bytes individually, we must check up to the previous 3 bytes as well */
    offset = prevend < 3 ? prevend : 3;
    chkptr = memmem(rctx->buf + prevend - offset, recvd + offset, "\r\n\r\n", 4);

    if(chkptr != NULL)
    {
        printf("[%zu] Headers complete.\n", idx);
        /* Make a null terminated string */
        *chkptr = 0;
        rctx->bufend = chkptr - rctx->buf;
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
    rctx->method = mhttp_interpret_method(rctx->buf);
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

    rctx->uri = strchr(rctx->buf, ' ') + 1;
    chkptr = strchr(rctx->uri, ' ');
    if(chkptr == NULL && (chkptr - rctx->buf) >= rctx->bufend)
    {
        goto malformed_err;
    }
    *chkptr = 0;
    mhttp_urldecode(rctx->uri, (char *) rctx->uri, chkptr+1 - rctx->uri);
    printf("[%zu] URI: %s\n", idx, rctx->uri);

    bufidx = (chkptr + 1) - rctx->buf;

    chkptr = strstr(rctx->buf + bufidx, "\r\n");
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
        bufidx = (chkptr - rctx->buf);
        chkptr = strstr(rctx->buf + bufidx, "\r\n");
    }

    switch(rctx->method)
    {
        case MHTTP_METHOD_GET:
        case MHTTP_METHOD_HEAD:
            if(stat(rctx->uri+1, &srcstat))
            {
                printf("[%zu] stat error: %s\n", idx, strerror(errno));
                if(errno == ENAMETOOLONG) { mhttp_send_error(fd, http414); }
                else if(errno == EACCES)  { mhttp_send_error(fd, http403); }
                else if(errno == ENOENT)  { mhttp_send_error(fd, http404); }
                else if(errno == ELOOP)   { mhttp_send_error(fd, http508); }
                else                      { mhttp_send_error(fd, http500); }
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
                    srcstat.st_size);
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
                    rctx->range.low, rctx->range.high, srcstat.st_size);
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
                /* Uses req context buffer - req->uri is now invalid. */
                rctx->bufend = 0;
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
    size_t chunklen, sent;
    /* Check if buffer is empty */
    if(rctx->bufend == 0)
    {
        chunklen = rctx->srclen < rctx->buflen ? rctx->srclen : rctx->buflen;
        sent = read(rctx->srcfd, rctx->buf, chunklen); /* Sent here means amount read. */
        rctx->srclen -= chunklen;
        if(sent != chunklen)
        {
            /* Huh? Early end of file? Set the EOS flag to bail gracefully */
            rctx->eos = true;
        }
        rctx->bufoff = 0;
        rctx->bufend = sent;
    }

    sent = send(fd, rctx->buf + rctx->bufoff, rctx->bufend, 0);
    rctx->bufoff += sent;
    rctx->bufend -= sent;

    if(!rctx->eos)
    {
        if(rctx->srclen == 0)
        {
            close(rctx->srcfd);
            mhttp_req_resetctx(rctx);
            mig_loop_setcall(lp, idx, mhttp_req_recv);
            mig_loop_setcond(lp, idx, MIG_COND_READ);
        }
    }
    else
    {
        mig_loop_unregister(lp, idx);
    }
}

void mhttp_send_dirindex(int fd, const char *dir)
{
    const size_t buflen = 4096;
    char buf[buflen];
    size_t written;
    DIR *dirp;
    struct dirent *dent;

    const char *preamble =
        "<!DOCTYPE html5>"
        "<head><title>%s</title></head>"
        "<body><h1>Directroy listing for %s</h1>"
            "<ul>";
    const char *postamble =
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

    bzero((void *) &addr, sizeof(addr));
    servsock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &ssopt_v, sizeof(ssopt_v));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(31337);
    if(bind(servsock, &addr, sizeof(addr)))
    {
        perror("bind");
        return 1;
    }
    listen(servsock, MAX_CONNS + 1);

    struct mig_loop *lp = mig_loop_create(MAX_CONNS + 1);
    mig_loop_register(lp, servsock, mhttp_accept, NULL, MIG_COND_READ, NULL);
    mig_loop_exec(lp);
    return 0;
}
