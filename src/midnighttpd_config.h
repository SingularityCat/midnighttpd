#ifndef MIDNIGHTTPD_CONFIG_H
#define MIDNIGHTTPD_CONFIG_H

#include <stdlib.h>
#include <stdbool.h>

#include "mig_core.h"
#include "mig_radix_tree.h"
#include "mig_dynarray.h"

#define MIDNIGHTTPD "midnighttpd"

#ifndef SECRECY
    #define SERVER_HEADER "Server: " MIDNIGHTTPD "\r\n"
#else
    #define SERVER_HEADER ""
#endif

#define TOKSTR(x) #x
#define stringify(x) TOKSTR(x)

#define DEFAULT_PORT    80

/* Default configurables */
#define LOOP_SLOTS          512
#define RX_BUFLEN           4096
#define TX_BUFLEN           4096
#define DIRINDEX_STATE      true
#define DIRINDEX_BUFLEN     4096
//#define DEFAULT_MIMETYPE    "application/octet-stream"
#define DEFAULT_MIMETYPE    "text/plain"

struct midnighttpd_config {
    char *root;
    size_t loop_slots;
    size_t rx_buflen;
    size_t tx_buflen;
    bool dirindex_enabled;
    size_t dirindex_buflen;

    struct mig_radix_tree *mimetypes;
    char *default_mimetype;
};

extern struct midnighttpd_config config;

bool sockaddr_parse(char **addr_p, char **port_p);
int cfg_bind(struct mig_dynarray *stk, char *addr);
int cfg_bindunix(struct mig_dynarray *stk, char *path);

void midnighttpd_configfile_read(const char *path, struct mig_dynarray *ent_stack, struct mig_dynarray *mem_stack);

#endif
