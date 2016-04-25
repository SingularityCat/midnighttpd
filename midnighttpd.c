#define _GNU_SOURCE
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mig_core.h"

#define MAX_CONNS 5

enum mhttp_range_spec
{
    MHTTP_RANGE_SPEC_NONE = 0x0,
    MHTTP_RANGE_SPEC_LOW,
    MHTTP_RANGE_SPEC_HIGH,
    MHTTP_RANGE_SPEC_BOTH
};


#define MHTTP_METHOD_UNSUPPORTED 0xF00
enum mhttp_method
{
    MHTTP_METHOD_NONE = 0x0,
    MHTTP_METHOD_GET,
    MHTTP_METHOD_HEAD,
    MHTTP_METHOD_OPTIONS,
    /* unsupported options, all start with 0x0F00 */
    MHTTP_METHOD_PUT = 0xF00,
    MHTTP_METHOD_DELETE,
    MHTTP_METHOD_PATCH,
    MHTTP_METHOD_POST
};

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
#define http501 "501 Not Implemented"
#define http503 "503 Service Unavailable"


#define allowed_methods "GET,HEAD,OPTIONS"
enum mhttp_method mhttp_interpret_method(char *meth)
{
    switch(*meth++)
    {
        case 'G': /* GET */
            if(strncmp("ET", meth, 2) == 0)
            {
                return MHTTP_METHOD_GET;
            }
            break;
        case 'H': /* HEAD */
            if(strncmp("EAD", meth, 3) == 0)
            {
                return  MHTTP_METHOD_HEAD;
            }
            break;
        case 'O': /* OPTIONS */
            if(strncmp("PTIONS", meth, 6) == 0)
            {
                return  MHTTP_METHOD_OPTIONS;
            }
            break;
        case 'P': /* P(UT|ATCHA|OST) */
            switch(*meth++)
            {
                case 'U': /* PUT */
                    if(*meth == 'T')
                    {
                        return MHTTP_METHOD_PUT;
                    }
                    break;
                case 'A': /* PATCH */
                    if(strncmp("TCH", meth, 3) == 0)
                    {
                        return MHTTP_METHOD_PATCH;
                    }
                    break;
                case 'O': /* POST */
                    if(strncmp("ST", meth, 2) == 0)
                    {
                        return MHTTP_METHOD_POST;
                    }
                    break;
            }
            break;
        case 'D': /* DELETE */
            if(strncmp("ELETE", meth, 5) == 0)
            {
                return MHTTP_METHOD_DELETE;
            }
            break;
    }
    return MHTTP_METHOD_NONE;
}


#define HDRBUF_MAX 2048

struct mhttp_req
{
    char buf[HDRBUF_MAX];
    size_t buflen;
    size_t bufend;
    size_t bufidx;
    enum mhttp_method method;
    const char *uri;
    struct {
        size_t low;
        size_t high;
        enum mhttp_range_spec spec;
    } range;
    bool eos;
    size_t srcentidx;
};

void mhttp_req_init(struct mig_loop *lp, size_t idx);
void mhttp_req_recv(struct mig_loop *lp, size_t idx);
void mhttp_req_intr(struct mig_loop *lp, size_t idx);
void mhttp_req_free(struct mig_loop *lp, size_t idx);
void mhttp_req_get(struct mig_loop *lp, size_t idx);
void mhttp_req_head(struct mig_loop *lp, size_t idx);
void mhttp_req_options(struct mig_loop *lp, size_t idx);

void mhttp_accept(struct mig_loop *lp, size_t idx)
{
    int sock = accept(mig_loop_getfd(lp, idx), NULL, NULL);
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
    size_t ni = mig_loop_register(lp, sock, mhttp_req_init, mhttp_req_free, MIG_COND_READ, NULL);
    if(ni == (size_t) -1)
    {
        char hdrbuf[HDRBUF_MAX];
        size_t hdrlen = snprintf(hdrbuf, HDRBUF_MAX,
            "HTTP/1.1 %s\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            http503);
        send(sock, hdrbuf, hdrlen, 0);
        close(sock);
        printf("[%zu] Rejecting new connection (no space left).\n", idx);
        return;
    }
    printf("[%zu] Accepting new connection as %zu\n", idx, ni);
}

void mhttp_req_free(struct mig_loop *lp, size_t idx)
{
    void *ctx = mig_loop_getdata(lp, idx);
    if(ctx != NULL)
    {
        free(ctx);
    }
    close(mig_loop_getfd(lp, idx));
}

void mhttp_req_resetctx(struct mhttp_req *rctx)
{
    rctx->bufend = 0;
    rctx->bufidx = 0;
    rctx->range.low = 0;
    rctx->range.high = -1;
    rctx->srcentidx = -1;
}

void mhttp_req_init(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = malloc(sizeof(*rctx));
    rctx->buflen = HDRBUF_MAX;
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

    if(rctx->bufend >= rctx->buflen)
    {
        /* Header too big. This is fatal. */
        char hdrbuf[HDRBUF_MAX];
        size_t hdrlen = snprintf(hdrbuf, HDRBUF_MAX,
            "HTTP/1.1 %s\r\n"
            "\r\n",
            http431);
        send(fd, hdrbuf, hdrlen, 0);
        mig_loop_unregister(lp, idx);
        return;
    }

    size_t recvd = recv(fd, rctx->buf + rctx->bufend, rctx->buflen - rctx->bufend, 0);
    rctx->bufend += recvd;
    printf("[%zu] recv'd %zu bytes of data.\n", idx, recvd);
    if(recvd == 0)
    {
        printf("[%zu] Connection closed.\n", idx);
        mig_loop_unregister(lp, idx);
    }

    chkptr = memmem(rctx->buf + rctx->bufidx, recvd, "\r\n\r\n", 4);
    if(chkptr != NULL)
    {
        /* switch to intr */
        mig_loop_setcall(lp, idx, mhttp_req_intr);
        mig_loop_setcond(lp, idx, MIG_COND_WRITE);
    }
}

void mhttp_req_intr(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    char *chkptr;
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    size_t sent;
    char hdrbuf[HDRBUF_MAX];
    size_t hdrlen = 0;

    /* Analyse client headers */
    rctx->method = mhttp_interpret_method(rctx->buf);
    if (rctx->method == MHTTP_METHOD_NONE)
    {
        hdrlen = snprintf(hdrbuf, HDRBUF_MAX,
            "HTTP/1.1 %s\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            http501);
        send(fd, hdrbuf, hdrlen, 0);
        mig_loop_unregister(lp, idx);
        return;
    }
    else if (rctx->method & MHTTP_METHOD_UNSUPPORTED)
    {
        hdrlen = snprintf(hdrbuf, HDRBUF_MAX,
            "HTTP/1.1 %s\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            http405);
        send(fd, hdrbuf, hdrlen, 0);
        mig_loop_unregister(lp, idx);
        return;
    }

    switch(rctx->method)
    {
        case MHTTP_METHOD_GET:
            mig_loop_setcall(lp, idx, mhttp_req_get);
            break;
        case MHTTP_METHOD_HEAD:
            mig_loop_setcall(lp, idx, mhttp_req_head);
            break;
        case MHTTP_METHOD_OPTIONS:
            mig_loop_setcall(lp, idx, mhttp_req_options);
            break;
    }

    rctx->bufidx = 0;
    chkptr = memmem(rctx->buf + rctx->bufidx, rctx->bufend - rctx->bufidx, "\r\n", 2) + 2;
    while(chkptr != NULL && *chkptr)
    {
        if(strncmp("Connection:", chkptr, 11) == 0)
        {
            chkptr += 11;
            while(*chkptr++ == ' '); /* Skip whitespace */
            rctx->eos = (strncmp("close", chkptr, 5) == 0);
        }
        else if(strncmp("Range:", chkptr, 6) == 0)
        {
            chkptr += 6;
            while(*chkptr++ == ' '); /* Skip whitespace */
            if(strncmp("bytes=", chkptr, 6))
            {
                rctx->range.low = 0;
                /* Extract lower number, if applicable. */
                while(isdigit(*chkptr))
                {
                    rctx->range.low *= 10;
                    rctx->range.low += *chkptr - 48;
                    chkptr++;
                }
                if(*chkptr != '-')
                {
                    /* Malformed header? */
                    goto malformed_err;
                }
                while(isdigit(*chkptr))
                {
                    rctx->range.high *= 10;
                    rctx->range.high += *chkptr - 48;
                    chkptr++;
                }
            }
        }
        rctx->bufidx = (chkptr - rctx->buf);
        chkptr = memmem(rctx->buf + rctx->bufidx, rctx->bufend - rctx->bufidx, "\r\n", 2) + 2;
    }

    return;

    malformed_err:
    hdrlen = snprintf(hdrbuf, HDRBUF_MAX,
        "HTTP/1.1 %s\r\n"
        "Content-Length: 0\r\n"
        "\r\n",
        http400);
    send(fd, hdrbuf, hdrlen, 0);
    mig_loop_unregister(lp, idx);
}

void mhttp_req_get(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    size_t sent;
    if(!rctx->eos)
    {
        mhttp_req_resetctx(rctx);
        mig_loop_setcall(lp, idx, mhttp_req_recv);
        mig_loop_setcond(lp, idx, MIG_COND_READ);
    }
    else
    {
        mig_loop_unregister(lp, idx);
    }
}

void mhttp_req_head(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    size_t sent;
    if(!rctx->eos)
    {
        mhttp_req_resetctx(rctx);
        mig_loop_setcall(lp, idx, mhttp_req_recv);
        mig_loop_setcond(lp, idx, MIG_COND_READ);
    }
    else
    {
        mig_loop_unregister(lp, idx);
    }
}

void mhttp_req_options(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    struct mhttp_req *rctx = mig_loop_getdata(lp, idx);
    char hdrbuf[HDRBUF_MAX];
    size_t hdrlen = 0;

    hdrlen = snprintf(hdrbuf, HDRBUF_MAX,
        "HTTP/1.1 %s\r\n"
        "Allow: %s\r\n"
        "\r\n",
        http204,
        allowed_methods);
    send(fd, hdrbuf, hdrlen, 0);
    
    if(!rctx->eos)
    {
        mhttp_req_resetctx(rctx);
        mig_loop_setcall(lp, idx, mhttp_req_recv);
        mig_loop_setcond(lp, idx, MIG_COND_READ);
    }
    else
    {
        mig_loop_unregister(lp, idx);
    }
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
