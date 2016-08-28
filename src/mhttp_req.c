#define _GNU_SOURCE

#include "mig_parse.h"

#include "mhttp_req.h"
#include "mhttp_req_header.h"

/* For reference:
 *
 * struct mhttp_req
 * {
 *     struct mig_buf rxbuf;
 *     struct mig_buf txbuf;
 *     enum mhttp_version version;
 *     enum mhttp_method method;
 *     const char *path;
 *     const char *args;
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
    req->version = MHTTP_VERSION_1_0;
    req->eos = true;
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
    req->method = 0;
    req->path = NULL;
    req->args = NULL;
    req->txenc = MHTTP_REQ_TXENC_NONE;
    req->entlen = 0;
    req->srcfd = -1;
    req->srclen = 0;
}


bool mhttp_req_check(struct mhttp_req *req, size_t offset)
{
    char *chkptr = NULL, *lf1, *lf2, *eob;
    /* RFC 2616 section 19.3 suggests tolerant applications should handle improper line termination.
     * So we look for *LF*LF rather then CRLFCRLF.
     */

    eob = req->rxbuf.base + req->rxbuf.end; /* end of (written) buffer. */

    /* As we can recv bytes individually, we must check some previous bytes as well */
    offset -= offset < 2 ? offset : 2;

    lf1 = memchr(req->rxbuf.base + offset, '\n', eob - (req->rxbuf.base + offset));
    while(lf1)
    {
        lf2 = memchr(lf1 + 1, '\n', eob - (lf1 + 1));
        if(lf2 && lf2 - lf1 < 3)
        {
            chkptr = lf2;
            break;
        }
        lf1 = lf2;
    }

    if(chkptr != NULL)
    {
        /* Make a null terminated string */
        *chkptr = 0;
        /* Set 'offset' of recv buffer to just after the end of this header. */
        req->rxbuf.off = (chkptr - req->rxbuf.base) + 1;
        return true;
    }

    return false;
}

bool mhttp_req_parse(struct mhttp_req *req)
{
    char *chkptr, *argptr;
    size_t pathlen, bufidx;

    /* Analyse client headers */
    req->method = mhttp_match_method(req->rxbuf.base, (const char **) &req->path);
    req->path++;

    argptr = strchr(req->path, '?');
    chkptr = strchr(req->path, ' ');
    if(chkptr == NULL) { goto malformed; }
    /* Split argument part of URL. */
    if(argptr == NULL || argptr > chkptr)
    {
        req->args = chkptr;
        pathlen = chkptr + 1 - req->path;
    }
    else if(argptr)
    {
        *argptr++ = 0;
        req->args = argptr;
        pathlen = argptr - req->path;
    }
    *chkptr++ = 0;
    mhttp_urldecode((char *) req->path, pathlen);
    mhttp_scrubpath((char *) req->path, false, NULL);
    *((char *) --req->path) = '.';

    /* Get HTTP version and set version specific defaults appropriately */
    if(strncmp(http_v1_1, chkptr, sizeof(http_v1_1) - 1) == 0)
    {
            req->version = MHTTP_VERSION_1_1;
            req->eos = false;
    }
    else
    {
            req->version = MHTTP_VERSION_1_0;
            req->eos = true;
    }

    bufidx = (chkptr + 1) - req->rxbuf.base;

    chkptr = strchr(req->rxbuf.base + bufidx, '\n'); /* RFC 2616 section 19.3 */
    while(chkptr != NULL)
    {
        char *pptr;
        enum mhttp_req_header hdr;
        chkptr++; /* Skip crlf */
        hdr = mhttp_req_match_header(chkptr, (const char **) &chkptr);
        while(*chkptr == ' ') { chkptr++; } /* Skip whitespace */
        switch(hdr)
        {
            case MHTTP_HEADER_CONNECTION:
                if(strncasecmp("close", chkptr, 5) == 0)
                {
                    req->eos = true;
                }
                else if(strncasecmp("keep-alive", chkptr, 9) == 0)
                {
                    req->eos = false;
                }
                break;
            case MHTTP_HEADER_RANGE:
                if(mhttp_parse_range(chkptr, &req->range)) { goto malformed; }
                break;
            case MHTTP_HEADER_CONTENT_LENGTH:
                req->entlen = mig_parse_uint_dec(chkptr, (const char **) &pptr);
                if(pptr)
                {
                    req->txenc = MHTTP_REQ_TXENC_IDENTITY;
                    chkptr = pptr;
                }
                break;
            case MHTTP_HEADER_TRANSFER_ENCODING:
                if(strncasecmp("chunked", chkptr, 7) == 0)
                {
                    req->txenc = MHTTP_REQ_TXENC_CHUNKED;
                }
                else if(strncasecmp("identity", chkptr, 8) == 0)
                {
                    req->txenc = MHTTP_REQ_TXENC_IDENTITY;
                }
                break;
        }
        bufidx = (chkptr - req->rxbuf.base);
        chkptr = strchr(req->rxbuf.base + bufidx, '\n');
    }

    return true;

    malformed:
    req->path = NULL;
    req->args = NULL;
    return false;
}

