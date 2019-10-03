#include <stdio.h>
#include <stdlib.h>      // for exit
#include <sys/types.h>
#include <sys/socket.h> // for socket
#include <arpa/inet.h>  // for htons, htonl
#include <string.h>     // for memset
#include <errno.h>      // for errno
#include <unistd.h>     // for write


#define SERV_PORT 5000
#define LISTENQ   1024
#define	MAXLINE	  1024


int open_listen_fd(int portnum);
int readn(int fd, char *buff, size_t nbytes);
int writen(int fd, char *buff, size_t nbytes);


int main(void)
{
	char buffer[MAXLINE];
	int serverfd, clientfd;
	socklen_t clilen;
	struct sockaddr_in cliaddr;

	serverfd = open_listen_fd(SERV_PORT);

	if (serverfd < 0)
		exit(1);

	for (;;) {
	   	clilen = sizeof(cliaddr);
		clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

		if (clientfd < 0) {
			fprintf(stderr, "Accept error: %s\n", strerror(errno));
			exit(1);
		}

	    printf("Connection from %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

	    if (readn(clientfd, buffer, sizeof(buffer)) < 0)
	    	exit(1);

	   	printf("Message: %s\n", buffer);

	    snprintf(buffer, sizeof(buffer), "Hello, brother\n");

		if (writen(clientfd, buffer, strlen(buffer)) < 0)
			exit(1);

	   close(clientfd);
	}

	return 0;
}

int open_listen_fd(int portnum)
{
	int sockfd, enable = 1;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "Socket error: %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		fprintf(stderr, "Setsockopt(SO_REUSEADDR) error: %s\n", strerror(errno));
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(portnum);

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "Bind error: %s\n", strerror(errno));
		return -1;
	}

	if (listen(sockfd, LISTENQ) < 0) {
		fprintf(stderr, "Listen error: %s\n", strerror(errno));
		return -1;
	}

	return sockfd;
}

int readn(int fd, char *buff, size_t nbytes)
{
	size_t nleft;
	ssize_t nreaded;

	nleft = nbytes-1;	/*-1 for '\0'*/

	while (nleft > 0) {
		nreaded = read(fd, buff, nleft);

		if (nreaded < 0) {
			fprintf(stderr, "Read error: %s\n", strerror(errno));
			return -1;
		} else if ((nreaded == 0) || strstr(buff, "\0"))
			break;

		nleft -= nreaded;
		buff += nreaded;
	}

	return nbytes - nleft;
}

int writen(int fd, char *buff, size_t nbytes)
{
	size_t nleft;
	ssize_t nwritten;

	nleft = nbytes+1;	/*+1 for '\0'*/

	while (nleft > 0) {
		if ((nwritten = write(fd, buff, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else {
				fprintf(stderr, "Write error: %s\n", strerror(errno));
				return -1;
			}
		}

		nleft -= nwritten;
		buff  += nwritten;
	}

	return nbytes;
}

