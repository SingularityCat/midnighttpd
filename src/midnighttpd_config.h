#ifndef MIDNIGHTTPD_CONFIG_H
#define MIDNIGHTTPD_CONFIG_H

#include <stdlib.h>
#include <stdbool.h>

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
#define LOOP_SLOTS      512
#define RX_BUFLEN       4096
#define TX_BUFLEN       4096
#define DIRINDEX_STATE  true
#define DIRINDEX_BUFLEN 4096

struct midnighttpd_config {
    size_t loop_slots;
    size_t rx_buflen;
    size_t tx_buflen;
    bool dirindex_enabled;
    size_t dirindex_buflen;
};

extern struct midnighttpd_config config;

#endif
