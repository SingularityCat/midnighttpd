#include <stdio.h>
#include <string.h>
#include "mig_core.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CONNS 5

int clients[MAX_CONNS];
size_t clidx = 0;


void chat(struct mig_loop *, size_t);

void mig_accept(struct mig_loop *lp, size_t idx)
{
    int sock = accept(mig_loop_getfd(lp, idx), NULL, NULL);
    size_t ni = mig_loop_register(lp, sock, chat, NULL, MIG_COND_READ, NULL);
    printf("[%zu] Accepting new connection as %zu\n", idx, ni);
    clients[clidx++] = sock;
}

void chat(struct mig_loop *lp, size_t idx)
{
    puts("Recving data.");
    char buf[4096];
    int fd = mig_loop_getfd(lp, idx);
    size_t recvd = recv(fd, buf, 4096, 0);
    for(size_t i = 0; i < MAX_CONNS; i++)
    {
        if(clients[i] != fd && clients[i] != -1)
        {
            send(clients[i], buf, recvd, 0);
        }
    }
}

int main(int argc, char **argv)
{

    for(size_t i = 0; i < MAX_CONNS; i++)
    {
        clients[i] = -1;
    }


    int servsock;
    struct sockaddr_in addr;

    bzero((void *) &addr, sizeof(addr));
    servsock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(servsock, SOL_SOCKET, SO_REUSEADDR, NULL, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(31337);
    bind(servsock, &addr, sizeof(addr));
    listen(servsock, MAX_CONNS + 1);

    struct mig_loop *lp = mig_loop_create(MAX_CONNS + 1);
    mig_loop_register(lp, servsock, mig_accept, NULL, MIG_COND_READ, NULL);
    mig_loop_exec(lp);
}
