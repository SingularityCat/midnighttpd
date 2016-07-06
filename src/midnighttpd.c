#define _GNU_SOURCE
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdalign.h>
#include <ctype.h>
#include <errno.h>

#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#include <dirent.h>

#include "mig_core.h"
#include "mig_io.h"
#include "mig_dynarray.h"
#include "mig_opt.h"
#include "mig_parse.h"
#include "mhttp_req.h"
#include "mhttp_status.h"

#include "midnighttpd_core.h"
#include "midnighttpd_config.h"

static struct mig_loop *loop;

void sigint_hndlr(int sig)
{
    printf("midnighttpd - terminating\n");
    mig_loop_terminate(loop);
}

int main(int argc, char **argv)
{
    int argn = 1;
    char default_addr[] = "0:8080";
    struct mig_dynarray *mem_stack = mig_dynarray_create();
    struct mig_dynarray *ent_stack = mig_dynarray_create();
    int naddrs = 0;

    mig_dynarray_init(mem_stack, sizeof(void *), alignof(void *), 16, MIG_DYNARRAY_DEFAULT);
    mig_dynarray_init(ent_stack, sizeof(struct mig_ent), alignof(struct mig_ent), 8, MIG_DYNARRAY_DEFAULT);

    /* Configuration initialisation. */
    config.root = strdup(".");
    config.default_mimetype = strdup(DEFAULT_MIMETYPE);
    config.mimetypes = mig_radix_tree_create();

    const char *usage = 
        "midnighttpd - http daemon\n"
        "usage:\n"
        " -c configuration\n"
        "       Load configuration from a file.\n"
        " -m mimetype [extension ...]\n"
        "       Set a mimetype for a given list of extensions\n"
        " -M mimetype\n"
        "       Set default mimetype. Default is " DEFAULT_MIMETYPE "\n"
        " -q\n"
        "       Disable directory indexing.\n"
        " -Q\n"
        "       Enable directory indexing.\n"
        " -l addr[:port]\n"
        "       Listen on a given address/port combination.\n"
        "       If port is unspecified, default to port 80.\n"
        "       If address is an IPv6 address, it should be\n"
        "       surrounded by [square brackets]. e.g. [::1]\n"
        " -u unixaddr\n"
        "       Listen on a unix domain socket\n"
        " -r bytes\n"
        "       Size of recieve buffer. Default is " stringify(RX_BUFLEN) "\n"
        " -t bytes\n"
        "       Size of transmission buffer. Default is " stringify(TX_BUFLEN) "\n"
        " -n slots\n"
        "       Number of loop slots. Default is " stringify(LOOP_SLOTS) "\n"
        " -h, -?\n"
        "       Show this help text.\n"
        "";

    struct mig_optcfg *opts = mig_optcfg_create();

    mig_setopt(opts, 'c', 1, 0);

    mig_setopt(opts, 'm', 1, -1);
    mig_setopt(opts, 'M', 1, 0);
    mig_setopt(opts, 'q', 0, 0);
    mig_setopt(opts, 'Q', 0, 0);
    mig_setopt(opts, 'l', 1, 0);
    mig_setopt(opts, 'u', 1, 0);

    mig_setopt(opts, 'r', 1, 0);
    mig_setopt(opts, 't', 1, 0);
    mig_setopt(opts, 'n', 1, 0);

    mig_setopt(opts, '?', 0, 0);
    mig_setopt(opts, 'h', 0, 0);
    int opt = mig_getopt(opts, &argc, &argv, &argn);
    char *e;
    long int n;
    while(opt != -1)
    {
        switch(opt)
        {
            case 0:
                free(config.root);
                config.root = strdup(argv[0]);
                break;
            case 'c':
                midnighttpd_configfile_read(argv[0], ent_stack, mem_stack);
                break;
            case 'm':
                e = argv[0];
                for(int i = 1; i < argn; i++)
                {
                    mig_radix_tree_insert(config.mimetypes,
                        (uint8_t *) argv[i], strlen(argv[i]), e);
                }
                break;
            case 'M':
                free(config.default_mimetype);
                config.default_mimetype = strdup(argv[0]);
                break;
            case 'q':
                config.dirindex_enabled = false;
                break;
            case 'Q':
                config.dirindex_enabled = true;
                break;
            case 'l':
                cfg_bind(ent_stack, argv[0]);
                break;
            case 'u':
                cfg_bindunix(ent_stack, argv[0]);
                break;
            case 'n':
                e = NULL;
                n = mig_parse_int(argv[0], (const char **) &e);
                if(e && n > 0)
                {
                    config.loop_slots = n;
                }
                break;
            case 'r':
            case 't':
                e = NULL;
                n = mig_parse_size(argv[0], (const char **) &e);
                if(e && n > 0)
                {
                    switch(opt)
                    {
                        case 'r': config.rx_buflen = n; break;
                        case 't': config.tx_buflen = n; break;
                    }
                }
                break;
            case '?':
            case 'h':
                puts(usage);
                return 1;
        }
        opt = mig_getopt(opts, &argc, &argv, &argn);
    }

    mig_optcfg_destroy(opts);

    chdir(config.root);
    loop = mig_loop_create(config.loop_slots);

    struct mig_ent ent;
    bind_init:

    while(mig_dynarray_pop(ent_stack, &ent))
    {
        listen(ent.fd, config.loop_slots);
        mig_loop_register(loop, ent.fd, ent.call, ent.free, MIG_COND_READ, ent.data);
        naddrs++;
    }

    if(naddrs < 1)
    {
        cfg_bind(ent_stack, default_addr);
        goto bind_init;
    }

    mig_dynarray_destroy(ent_stack);

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sigint_hndlr);

    int status = 0;

    if(mig_loop_exec(loop))
    {
        printf("midnighttpd error - loop failed: %s\n", strerror(errno));
        status = -1;
    }

    /* Free allocated memory. */
    mig_loop_destroy(loop);
    mig_radix_tree_destroy(config.mimetypes);

    free(config.default_mimetype);
    free(config.root);

    void *mem;
    while(mig_dynarray_pop(mem_stack, &mem))
    {
        free(mem);
    }
    mig_dynarray_destroy(mem_stack);

    return status;
}
