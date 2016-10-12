#ifndef MHTTP_REQ_H
#define MHTTP_REQ_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "mig_io.h"
#include "mhttp_util.h"
#include "mhttp_status.h"
#include "mhttp_method.h"
#include "mhttp_range.h"

enum mhttp_req_txenc
{
    MHTTP_REQ_TXENC_NONE = 0,
    MHTTP_REQ_TXENC_IDENTITY = 1,
    MHTTP_REQ_TXENC_CHUNKED
};

struct mhttp_req
{
    struct mig_buf rxbuf;
    struct mig_buf txbuf;
    enum mhttp_version version;
    enum mhttp_method method;
    const char *path;
    const char *args;
    struct mhttp_range range;
    enum mhttp_req_txenc txenc;
    size_t entlen;
    bool eos;
    void *ext;

    int srcfd;
    size_t srclen;
};

struct mhttp_req *mhttp_req_create(size_t rxlen, size_t txlen, size_t exlen);
void mhttp_req_destroy(struct mhttp_req *req);

void mhttp_req_reset(struct mhttp_req *req);
bool mhttp_req_check(struct mhttp_req *req, size_t offset);
bool mhttp_req_parse(struct mhttp_req *req);

#endif
