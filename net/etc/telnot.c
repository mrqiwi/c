/* like telnet, but not telnet */
/* Usage: telnot hostname port */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <arpa/inet.h>

#include "../client_server/functions.h"

int main(int argc, char *argv[])
{
	int sock, res;
	struct addrinfo hints, *servinfo, *p;
    char hoststr[INET6_ADDRSTRLEN], portstr[PORTLEN];

    if (argc != 3) {
	    printf("usage: telnot hostname port\n");
	    exit(EXIT_FAILURE);
	}

	char *hostname = argv[1];
	char *port = argv[2];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((res = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(res));
		exit(EXIT_FAILURE);
	}

	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("socket");
			continue;
		}

		if (connect(sock, p->ai_addr, p->ai_addrlen) == -1) {
			perror("connect");
            close(sock);
			continue;
		}

		break;
	}

	if (!p) {
		printf( "client: failed to connect\n");
		exit(EXIT_FAILURE);
	}

    getnameinfo((struct sockaddr *)p->ai_addr, p->ai_addrlen, hoststr, sizeof hoststr,
    portstr, sizeof portstr, NI_NUMERICHOST | NI_NUMERICSERV);
    printf("connected to %s:%s\n", hoststr, portstr);
	printf("hit ^C to exit\n");

	freeaddrinfo(servinfo);

	struct pollfd fds[2];

	fds[0].fd = 0;
	fds[0].events = POLLIN;

	fds[1].fd = sock;
	fds[1].events = POLLIN;

	for(;;) {
		if (poll(fds, 2, -1) == -1) {
			perror("poll");
			exit(1);
		}

		for (int i = 0; i < 2; i++) {
			if (fds[i].revents & POLLIN) {
				int readbytes, writebytes;
				char buf[BUFSIZE];

				int outfd = fds[i].fd == 0 ? sock: 1;

				if ((readbytes = read(fds[i].fd, buf, BUFSIZE)) == -1) {
					perror("read");
					exit(1);
				}

				char *p = buf;
				int remainingbytes = readbytes;

				while (remainingbytes > 0) {
					if ((writebytes = write(outfd, p, remainingbytes)) == -1) {
						perror("write");
						exit(1);
					}

					p += writebytes;
					remainingbytes -= writebytes;
				}
			}
		}
	}
    exit(EXIT_SUCCESS);
}

