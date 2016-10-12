#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

#include "mig_io.h"
#include "mig_parse.h"
#include "mig_dynarray.h"

#include "midnighttpd_core.h"
#include "midnighttpd_config.h"
#include "midnighttpd_config_opt.h"

struct midnighttpd_config config  = {
    NULL,
    LOOP_SLOTS,
    RX_BUFLEN,
    TX_BUFLEN,
    EX_STRUCTLEN,
    DIRINDEX_STATE,
    DIRINDEX_BUFLEN,
    NULL,
    NULL
};


/* Socket addresses have the form:
 * <socket addr> ::= <addr> ":" <port> |
 *                   <ipv4 addr> |
 *                   <ipv6 addr> |
 *                   <unix addr>
 *
 * <addr> ::= <ipv4 addr> |
 *            "[" <ipv6 addr> "]"
 * <port> ::= <integer>
 *
 */

bool sockaddr_parse(char **addr_p, char **port_p)
{
    bool retval = true;
    char *port =  strrchr(*addr_p, ':');
    char *lbrack = strchr(*addr_p, '[');
    char *rbrack = strchr(*addr_p, ']');

    if(port && lbrack && rbrack && rbrack < port)
    {
        /* Address is a [ipv6]:port address. */
        *port_p = port + 1;
        *port = 0;
        *lbrack = 0;
        *rbrack = 0;
        *addr_p = lbrack + 1;
    }
    else if(port && lbrack == NULL && rbrack == NULL)
    {
        /* Address is a ipv4:port address */
        *port_p = port + 1;
        *port = 0;
    }
    else if(lbrack && port < rbrack)
    {
        /* Address is a [ipv6] address */
        *port_p = NULL;
        *lbrack = 0;
        *rbrack = 0;
        *addr_p = lbrack + 1;
    }
    else if(port == NULL && lbrack == NULL && rbrack == NULL)
    {
        /* Address is a ipv4 address */
        *port_p = NULL;
    }
    else
    {
        /* Address is malformed */
        *addr_p = NULL;
        *port_p = NULL;
        retval = false;
    }

    return retval;
}

int cfg_bind(struct mig_dynarray *stk, char *addr)
{
    int ncon = 0;
    int ssopt_v = 1;
    int servsock;
    struct mig_ent ent;

    char *port;

    if(!sockaddr_parse(&addr, &port))
    {
        return -1;
    }

    if(port == NULL)
    {
        port = stringify(DEFAULT_PORT);
    }

    printf("midnighttpd - binding to %s on port %s\n", addr, port);

    int gairet;
    struct addrinfo aih, *aip, *aic;
    memset(&aih, 0, sizeof(struct addrinfo));
    aih.ai_family = AF_UNSPEC;
    aih.ai_socktype = SOCK_STREAM;
    aih.ai_flags = AI_PASSIVE | AI_V4MAPPED | AI_ADDRCONFIG;
    aih.ai_protocol = 0;
    aih.ai_canonname = NULL;
    aih.ai_addr = NULL;
    aih.ai_next = NULL;

    gairet = getaddrinfo(addr, port, &aih, &aip);
    if(gairet)
    {
        printf("midnighttpd error - getaddrinfo: %s\n", gai_strerror(gairet));
        return -1;
    }

    aic = aip;
    while(aic)
    {
        servsock = socket(aic->ai_family, aic->ai_socktype, 0);
        setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, &ssopt_v, sizeof(ssopt_v));
        if(bind(servsock, aic->ai_addr, aic->ai_addrlen))
        {
            printf("midnighttpd error - bind: %s\n", strerror(errno));
            close(servsock);
            aic = aic->ai_next;
            continue;
        }
        ent.call = listen_accept;
        ent.free = listen_close_sock;
        ent.data = NULL;
        ent.fd = servsock;
        mig_dynarray_push(stk, &ent);
        ncon++;
        aic = aic->ai_next;
    }
    freeaddrinfo(aip);
    return ncon > 0 ? 0 : -1;
}

int cfg_bindunix(struct mig_dynarray *stk, char *opath)
{
    int cwd = open(".", 0);
    char *path = opath, *dirslash;
    int servsock;
    struct mig_ent ent;
    struct stat sockstat;
    struct sockaddr_un unixaddr;

    if(cwd == -1) { return -1; }

    dirslash = strrchr(opath, '/');
    if(dirslash)
    {
        *dirslash = 0;
        if(chdir(opath)) { goto err; }
        *dirslash = '/';
        path = dirslash + 1;
    }

    memset(&unixaddr, 0, sizeof(struct sockaddr_un));
    unixaddr.sun_family = AF_UNIX;
    strncpy(unixaddr.sun_path, path, sizeof(((struct sockaddr_un *) NULL)->sun_path) - 1);
    printf("midnighttpd - binding to unix domain socket %s\n", path);

    retry_stat:
    if(stat(path, &sockstat))
    {
        if(errno == EINTR)
        {
            goto retry_stat;
        }
        else if(errno != ENOENT)
        {
            goto err;
        }
    }
    else if(S_ISSOCK(sockstat.st_mode))
    {
        unlink(path);
    }

    servsock = socket(AF_UNIX, SOCK_STREAM, 0);
    mode_t msk = umask(0117); /* Ensure 0660 permissions. */
    int ret = bind(servsock, (struct sockaddr *) &unixaddr, sizeof(struct sockaddr_un));
    umask(msk);
    if(ret)
    {
        printf("midnighttpd error - bind: %s\n", strerror(errno));
        close(servsock);
        return -1;
    }
    ent.call = listen_accept;
    ent.free = listen_close_sockunix;
    ent.data = realpath(opath, NULL);
    ent.fd = servsock;
    mig_dynarray_push(stk, &ent);

    fchdir(cwd);
    close(cwd);
    return 0;

    err:
    fchdir(cwd);
    close(cwd);
    return -1;
}

void midnighttpd_configfile_read(const char *path, struct mig_dynarray *ent_stack, struct mig_dynarray *mem_stack)
{
    int cwd = open(".", 0);
    char *rpath, *dirslash;
    struct mig_buf fbuf = {NULL, 0, 0, 0};
    char *line;
    char *tok, *etok, *e;

    if(cwd == -1) { return; }

    rpath = realpath(path, NULL);
    if(!rpath) { return; }

    dirslash = strrchr(rpath, '/');
    if(!dirslash) { goto err; }
    *dirslash = 0;
    if(chdir(rpath)) { goto err; }
    *dirslash = '/';

    mig_buf_loadfile(&fbuf, dirslash + 1);
    line = fbuf.base;

    while(line)
    {
        enum midnighttpd_config_opt cfgopt = midnighttpd_match_config_opt(line, (const char **) &tok);
        while(*tok == ' ' || *tok == '\t') { tok++; }
        line = strchr(tok, '\n');
        if(line != NULL)
        {
            *line++ = 0;
        }

        switch(cfgopt)
        {
            case MIDNIGHTTPD_CONFIG_INCLUDE:
                midnighttpd_configfile_read(tok, ent_stack, mem_stack);
                break;

            case MIDNIGHTTPD_CONFIG_ROOT:
                free(config.root);
                config.root = strdup(tok);
                break;

            case MIDNIGHTTPD_CONFIG_LISTEN:
                cfg_bind(ent_stack, tok);
                break;

            case MIDNIGHTTPD_CONFIG_LISTEN_UNIX:
                cfg_bindunix(ent_stack, tok);
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
    }

    free(fbuf.base);
    err:
    free(rpath);
    fchdir(cwd);
    close(cwd);
}
