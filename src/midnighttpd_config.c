#include <string.h>

#include "mig_io.h"
#include "mig_parse.h"
#include "mig_dynarray.h"

#include "midnighttpd_config.h"
#include "midnighttpd_config_opt.h"

struct midnighttpd_config config  = {
    NULL,
    LOOP_SLOTS,
    RX_BUFLEN,
    TX_BUFLEN,
    DIRINDEX_STATE,
    DIRINDEX_BUFLEN,
    NULL,
    NULL
};


void midnighttpd_configfile_read(const char *path, struct mig_dynarray *ent_stack, struct mig_dynarray *mem_stack)
{
    struct mig_buf fbuf = {NULL, 0, 0, 0};
    mig_buf_loadfile(&fbuf, path);
    char *line = fbuf.base;
    char *tok, *etok, *e;
    enum midnighttpd_config_opt cfgopt = midnighttpd_match_config_opt(line, (const char **) &tok);
    while(cfgopt)
    {
        while(*tok == ' ') { tok++; }
        line = strchr(tok, '\n');
        if(line != NULL)
        {
            *line++ = 0;
        }

        switch(cfgopt)
        {
            case MIDNIGHTTPD_CONFIG_ROOT:
                free(config.root);
                config.root = strdup(tok);
                break;

            case MIDNIGHTTPD_CONFIG_LISTEN:
                (ent_stack, tok);
                break;

            case MIDNIGHTTPD_CONFIG_LISTEN_UNIX:
                (ent_stack, tok);
                break;

            case MIDNIGHTTPD_CONFIG_DEFAULT_MIMETYPE:
                tok = strtok_r(tok, " ", &etok);
                free(config.default_mimetype);
                config.default_mimetype = strdup(tok);
                break;

            case MIDNIGHTTPD_CONFIG_MIMETYPE:
                e = strdup(strtok_r(tok, " ", &etok)); /* The mimetype */
                mig_dynarray_push(mem_stack, &e);
                tok = strtok_r(NULL, " ", &etok);
                while(tok != NULL)
                {
                    mig_radix_tree_insert(config.mimetypes,
                        (uint8_t *) tok, strlen(tok), e);
                    tok = strtok_r(NULL, " ", &etok);
                }
                break;

            case MIDNIGHTTPD_CONFIG_DIRINDEX:
                config.dirindex_enabled = mig_parse_bool(tok, NULL);
                break;

            case MIDNIGHTTPD_CONFIG_LOOP_SLOTS:
                config.loop_slots = mig_parse_int(tok, NULL);
                break;

            case MIDNIGHTTPD_CONFIG_RXBUF:
                config.rx_buflen = mig_parse_size(tok, NULL);
                break;

            case MIDNIGHTTPD_CONFIG_TXBUF:
                config.tx_buflen = mig_parse_size(tok, NULL);
                break;
        }
        cfgopt = midnighttpd_match_config_opt(line, (const char **) &tok);
    }

    free(fbuf.base);
}
