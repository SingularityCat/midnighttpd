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

struct midnighttpd_config {
    size_t loop_slots;
    size_t header_buflen;
    bool dirlst_enabled;
    size_t dirlst_buflen;
};

extern struct midnighttpd_config config;

#endif
