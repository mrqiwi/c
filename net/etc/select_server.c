#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include "functions.h"

int main(void)
{
	char buffer[MAXLINE];
    char hoststr[INET_ADDRSTRLEN], portstr[PORTLEN];
	socklen_t clilen;
    struct sockaddr_storage cliaddr;
	int serverfd, clientfd, i, j, nbytes, fdmax;
    fd_set master, read_fds;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

	serverfd = open_listen_fd(SERV_PORT);

	if (serverfd < 0)
		exit(1);

    FD_SET(serverfd, &master);
    fdmax = serverfd;

	for (;;) {
		read_fds = master;

        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select error");
            exit(1);
        }

        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == serverfd) {
                    clilen = sizeof cliaddr;
					clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

					if (clientfd < 0) {
                        perror("accept error");
                    } else {
                        FD_SET(clientfd, &master);

                        if (clientfd > fdmax)
                            fdmax = clientfd;

                        getnameinfo((struct sockaddr *)&cliaddr, clilen, hoststr, sizeof hoststr,
                        portstr, sizeof portstr, NI_NUMERICHOST | NI_NUMERICSERV);
                        printf("new connection from %s:%s on socket %d\n", hoststr, portstr, clientfd);
                    }
                } else {
                    if ((nbytes = recv(i, buffer, sizeof buffer, 0)) <= 0) {
                        if (nbytes == 0)
                            printf("selectserver: socket %d hung up\n", i);
                        else
                            perror("recv");

                        close(i);
                        FD_CLR(i, &master);
                    } else {
                        for (j = 0; j <= fdmax; j++) {
                            if (FD_ISSET(j, &master)) {
                                if (j != serverfd && j != i) {
                                    if (sendall(j, buffer, nbytes) == -1) {
                                        perror("send");
                                    }
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

