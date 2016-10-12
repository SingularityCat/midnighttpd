#ifndef MIDNIGHTTPD_CORE_H
#define MIDNIGHTTPD_CORE_H

#include <stdio.h>

#include "mig_core.h"
#include "mhttp_req.h"

#ifdef DEBUG
    #define debug(fmt, ...) fprintf(stderr, (fmt), (__VA_ARGS__))
#else
    #define debug(fmt, ...)
#endif

void listen_accept(struct mig_loop *lp, size_t idx);
void conn_free(struct mig_loop *lp, size_t idx);
void conn_init(struct mig_loop *lp, size_t idx);

void req_recv_headers(struct mig_loop *lp, size_t idx);
void req_read_headers(struct mig_loop *lp, size_t idx);

void resp_send_file(struct mig_loop *lp, size_t idx);
void resp_send_dirindex(int fd, struct mhttp_req *rctx);
void conn_terminate(struct mig_loop *lp, size_t idx, struct mhttp_req *req);

void listen_close_sock(struct mig_loop *lp, size_t idx);
void listen_close_sockunix(struct mig_loop *lp, size_t idx);

#endif
