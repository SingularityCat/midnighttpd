#include "midnighttpd_config.h"

struct midnighttpd_config config  = {
    LOOP_SLOTS,
    RX_BUFLEN,
    TX_BUFLEN,
    DIRINDEX_STATE,
    DIRINDEX_BUFLEN,
    NULL,
    DEFAULT_MIMETYPE
};
