#include <stdio.h>
#include <string.h>
#include "mig_core.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CONNS 5

int clients[MAX_CONNS];
size_t clidx = 0;


void tchat_msg(struct mig_loop *, size_t);
void tchat_free(struct mig_loop *, size_t);

void tchat_accept(struct mig_loop *lp, size_t idx)
{
    int sock = accept(mig_loop_getfd(lp, idx), NULL, NULL);
    if(clidx == MAX_CONNS)
    {
        close(sock);
        printf("[%zu] Rejecting new connection (no space left).\n", idx);
        return;
    }
    size_t ni = mig_loop_register(lp, sock, tchat_msg, tchat_free, MIG_COND_READ, NULL);
    printf("[%zu] Accepting new connection as %zu\n", idx, ni);
    clients[clidx++] = sock;
}

void tchat_msg(struct mig_loop *lp, size_t idx)
{
    char buf[4096];
    int fd = mig_loop_getfd(lp, idx);
    size_t recvd = recv(fd, buf, 4096, 0);
    printf("[%zu] Recv'd %zu bytes of data.\n", idx, recvd);
    if(recvd == 0)
    {
        printf("[%zu] Closing connection.\n", idx);
        mig_loop_unregister(lp, idx);
    }

    for(size_t i = 0; i <= clidx; i++)
    {
        if(clients[i] != fd && clients[i] != -1)
        {
            send(clients[i], buf, recvd, 0);
        }
    }
}

void tchat_free(struct mig_loop *lp, size_t idx)
{
    int fd = mig_loop_getfd(lp, idx);
    close(fd);
    size_t i;
    for(i = 0; i < clidx; i++)
    {
        if(clients[i] == fd)
        {
            break;
        }
    }
    for(i; i < (clidx - 1); i++)
    {
        clients[i] = clients[i + 1];
    }
    clients[clidx] = -1;
    clidx--;
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
    bind(servsock, (struct sockaddr *) &addr, sizeof(addr));
    listen(servsock, MAX_CONNS + 1);

    struct mig_loop *lp = mig_loop_create(MAX_CONNS + 1);
    mig_loop_register(lp, servsock, tchat_accept, NULL, MIG_COND_READ, NULL);
    mig_loop_exec(lp);
}
