#define _GNU_SOURCE
#include "mhttp_req.h"

/* For reference:
 *
 * struct mhttp_req
 * {
 *     struct mig_buf rxbuf;
 *     struct mig_buf txbuf;
 *     enum mhttp_method method;
 *     const char *uri;
 *     const char *arg;
 *     struct mhttp_range range;
 *     bool eos;
 *     int srcfd;
 *     size_t srclen;
 * };
 */
struct mhttp_req *mhttp_req_create(size_t rxlen, size_t txlen);
void mhttp_req_destroy(struct mhttp_req *req);

void mhttp_req_reset(struct mhttp_req *req);
bool mhttp_req_check(struct mhttp_req *req, size_t offset);
bool mhttp_req_parse(struct mhttp_req *req);

struct mhttp_req *mhttp_req_create(size_t rxlen, size_t txlen)
{
    struct mhttp_req *req = malloc(sizeof(struct mhttp_req) + rxlen + txlen + 1);
    req->rxbuf.base = (char *) (req + 1);
    req->rxbuf.len = rxlen;
    req->txbuf.base = req->rxbuf.base + req->rxbuf.len;
    req->txbuf.len = txlen;
    mig_buf_empty(&req->rxbuf);
    mig_buf_empty(&req->txbuf);
    mhttp_req_reset(req);
    /* A single null byte to cap the buffers. */
    *(req->txbuf.base + req->txbuf.len) = 0;
    return req;
}


void mhttp_req_destroy(struct mhttp_req *req)
{
    if(req->srcfd != -1)
    {
        close(req->srcfd);
    }
    free(req);
}


void mhttp_req_reset(struct mhttp_req *req)
{
    req->txbuf.end = 0;
    req->txbuf.off = 0;
    req->range.low = 0;
    req->range.high = -1;
    req->range.spec = MHTTP_RANGE_SPEC_NONE;
    req->eos = false;
    req->srcfd = -1;
    req->srclen = 0;
}


bool mhttp_req_check(struct mhttp_req *req, size_t offset)
{
    char *chkptr;

    /* As we can recv bytes individually, we must check up to the previous 3 bytes as well */
    offset -= offset < 3 ? offset : 3;
    chkptr = memmem(req->rxbuf.base + offset, req->rxbuf.end - offset, "\r\n\r\n", 4);

    if(chkptr != NULL)
    {
        /* Make a null terminated string */
        *chkptr = 0;
        /* Set 'offset' of recv buffer to just after the end of this header. */
        req->rxbuf.off = (chkptr - req->rxbuf.base) + 4;
        return true;
    }

    return false;
}

bool mhttp_req_parse(struct mhttp_req *req)
{
    char *chkptr, *argptr;
    size_t bufidx;

    /* Analyse client headers */
    req->method = mhttp_match_method(req->rxbuf.base, (const char **) &req->uri);

    argptr = strchr(req->uri, '?');
    chkptr = strchr(req->uri, ' ');
    if(chkptr == NULL) { goto malformed; }
    /* Split argument part of URL. */
    if(argptr == NULL || argptr > chkptr)
    {
        req->arg = chkptr;
    }
    else if(argptr)
    {
        *argptr++ = 0;
        req->arg = argptr;
    }
    *chkptr = 0;
    mhttp_urldecode(req->uri, (char *) req->uri, req->arg+1 - req->uri);

    bufidx = (chkptr + 1) - req->rxbuf.base;

    chkptr = strstr(req->rxbuf.base + bufidx, "\r\n");
    while(chkptr != NULL)
    {
        chkptr += 2; /* Skip crlf */
        if(strncmp("Connection:", chkptr, 11) == 0)
        {
            chkptr += 11;
            while(*chkptr++ == ' '); /* Skip whitespace */
            req->eos = (strncmp("close", chkptr, 5) == 0);
        }
        else if(strncmp("Range:", chkptr, 6) == 0)
        {
            chkptr += 6;
            if(mhttp_parse_range(chkptr, &req->range)) { goto malformed; }
        }
        bufidx = (chkptr - req->rxbuf.base);
        chkptr = strstr(req->rxbuf.base + bufidx, "\r\n");
    }

    return true;

    malformed:
    req->uri = NULL;
    req->arg = NULL;
    return false;
}

