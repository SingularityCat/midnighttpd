#ifndef MHTTP_REQ_H
#define MHTTP_REQ_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>

#include "mig_core.h"
#include "mig_io.h"
#include "mhttp_util.h"
#include "mhttp_method.h"
#include "mhttp_range.h"

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

struct mhttp_req *mhttp_req_create(size_t rxlen, size_t txlen);
void mhttp_req_destroy(struct mhttp_req *req);

void mhttp_req_reset(struct mhttp_req *req);
bool mhttp_req_check(struct mhttp_req *req, size_t offset);
bool mhttp_req_parse(struct mhttp_req *req);

#endif
