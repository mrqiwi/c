#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#include "functions.h"

void add_to_pfds(struct pollfd *pfds[], int newfd, int *fd_count, int *fd_size)
{
    // If we don't have room, add more space in the pfds array
    if (*fd_count == *fd_size) {
        *fd_size *= 2; // Double it

        *pfds = realloc(*pfds, sizeof(**pfds) * (*fd_size));
    }

    (*pfds)[*fd_count].fd = newfd;
    (*pfds)[*fd_count].events = POLLIN; // Check ready-to-read

    (*fd_count)++;
}

void del_from_pfds(struct pollfd pfds[], int i, int *fd_count)
{
    // Copy the one from the end over this one
    pfds[i] = pfds[*fd_count-1];

    (*fd_count)--;
}

int main(void)
{
    char buffer[MAXLINE];
    char hoststr[INET_ADDRSTRLEN];
    char portstr[PORTLEN];
    int serverfd, clientfd;
    int sender_fd, dest_fd;
    int i, j, nbytes;
    socklen_t clilen;
    struct sockaddr_storage cliaddr;
    int fd_count = 1, fd_size = 5;
    struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

    serverfd = open_listen_fd(SERV_PORT);

    if (serverfd < 0)
        exit(1);

    pfds[0].fd = serverfd;
    pfds[0].events = POLLIN;

    for (;;) {
        if (poll(pfds, fd_count, -1) < 0) {
            perror("poll error");
            exit(1);
        }

        for (i = 0; i < fd_count; i++) {
            if (pfds[i].revents & POLLIN) {
                if (pfds[i].fd == serverfd) {
                    clilen = sizeof cliaddr;
                    clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

                    if (clientfd < 0) {
                        perror("accept error");
                    } else {
                        add_to_pfds(&pfds, clientfd, &fd_count, &fd_size);

                        getnameinfo((struct sockaddr *)&cliaddr, clilen, hoststr, sizeof hoststr,
                        portstr, sizeof portstr, NI_NUMERICHOST | NI_NUMERICSERV);
                        printf("new connection from %s:%s on socket %d\n", hoststr, portstr, clientfd);
                    }
                } else {
                    nbytes = recv(pfds[i].fd, buf, sizeof buf, 0);
                    sender_fd = pfds[i].fd;

                    if (nbytes <= 0) {
                        if (nbytes == 0)
                            printf("pollserver: socket %d hung up\n", sender_fd);
                        else
                            perror("recv");

                        close(pfds[i].fd);
                        del_from_pfds(pfds, i, &fd_count);

                    } else {
                        for(j = 0; j < fd_count; j++) {
                            dest_fd = pfds[j].fd;

                            if (dest_fd != serverfd && dest_fd != sender_fd) {
                                if (sendall(dest_fd, buf, nbytes, 0) == -1) {
                                    perror("send error");
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
