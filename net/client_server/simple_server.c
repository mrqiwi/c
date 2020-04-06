#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#include "functions.h"

int main(void)
{
	char buffer[BUFSIZE] = {0};
    char hoststr[INET_ADDRSTRLEN], portstr[INET_ADDRSTRLEN];
	int serverfd, clientfd;
	socklen_t clilen;
    struct sockaddr_storage cliaddr;

	serverfd = open_listen_fd(SERV_PORT);

	if (serverfd < 0)
		exit(1);

	for (;;) {
	   	clilen = sizeof cliaddr;
        clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

		if (clientfd < 0) {
			perror("accept error");
			exit(1);
		}

        getnameinfo((struct sockaddr *)&cliaddr, clilen, hoststr, sizeof(hoststr),
        portstr, sizeof(portstr),NI_NUMERICHOST | NI_NUMERICSERV);

        printf("new connection from %s:%s\n", hoststr, portstr);

	    if (readn(clientfd, buffer, sizeof(buffer)) < 0)
	    	exit(1);

	   	printf("message: %s\n", buffer);

	    snprintf(buffer, sizeof(buffer), "Hello, brother");

		if (sendall(clientfd, buffer, strlen(buffer)) < 0)
			exit(1);

	   close(clientfd);
	}

	return 0;
}


