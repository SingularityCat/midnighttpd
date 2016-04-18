#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mig_core.h"

#define MAX_CONNS 5

const char http503[] = "HTTP/1.0 503 Service Unavailable\r\n\r\n";
#define http503len sizeof(http503)
const char http431[] = "HTTP/1.0 431 Request Header Fields Too Large\r\n\r\n";
#define http431len sizeof(http431)

enum mhttp_method {
    MHTTP_METHOD_NONE = 0,
    MHTTP_METHOD_GET,
    MHTTP_METHOD_HEAD,
    MHTTP_METHOD_OPTIONS,
    /* only supported options */
};

struct mhttp_req {
    char buf[2048];
    size_t buflen;
    size_t bufend;
    size_t bufidx;
    enum mhttp_method method;
    const char *uri;
    struct {
        size_t low;
        size_t high;
    } range;
    size_t srcentidx;
};

enum mhttp_method mhttp_str2method(char *meth)
{
    switch(*meth++)
    {
        case 'G': /* GET */
            return MHTTP_METHOD_GET;
        case 'H': /* HEAD */
            return  MHTTP_METHOD_HEAD;
        case 'O': /* OPTIONS */
            return  MHTTP_METHOD_OPTIONS;
    }
    return MHTTP_METHOD_NONE;
}

void mhttp_req_init(struct mig_loop *lp, size_t idx);
void mhttp_req_recv(struct mig_loop *lp, size_t idx);
void mhttp_req_intr(struct mig_loop *lp, size_t idx);
void mhttp_req_free(struct mig_loop *lp, size_t idx);

void mhttp_accept(struct mig_loop *lp, size_t idx)
{
    int sock = accept(mig_loop_getfd(lp, idx), NULL, NULL);
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK);
    size_t ni = mig_loop_register(lp, sock, mhttp_req_init, mhttp_req_free, MIG_COND_READ, NULL);
    if(ni == (size_t) -1)
    {
        send(sock, http503, http503len, 0);
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
    rctx->buflen = 2048;
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
        send(fd, http431, http431len, 0);
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

    const char rspl[] = "HTTP/1.1 200 OK\r\n\r\n";
    sent = send(fd, rspl, sizeof(rspl), 0);
    sent += send(fd, rctx->buf, rctx->bufend, 0);
    sent += send(fd, "\r\n\r\n", 4, 0);

    mhttp_req_resetctx(rctx);
    mig_loop_setcall(lp, idx, mhttp_req_recv);
    mig_loop_setcond(lp, idx, MIG_COND_READ);
}

int main(int argc, char **argv)
{
    int servsock;
    struct sockaddr_in addr;

    bzero((void *) &addr, sizeof(addr));
    servsock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(31337);
    bind(servsock, &addr, sizeof(addr));
    listen(servsock, MAX_CONNS + 1);

    struct mig_loop *lp = mig_loop_create(MAX_CONNS + 1);
    mig_loop_register(lp, servsock, mhttp_accept, NULL, MIG_COND_READ, NULL);
    mig_loop_exec(lp);
}
