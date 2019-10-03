#include <stdio.h>
#include <stdlib.h>     //for exit
#include <sys/types.h>
#include <sys/socket.h> // for socket
#include <arpa/inet.h>  // for htons, htonl
#include <string.h>		// for memset
#include <errno.h>		// for errno
#include <unistd.h>		// for write


#define SERV_ADDR "127.0.0.1"
#define SERV_PORT 5000
#define	MAXLINE	  1024


int open_connection(int portnum, char *address);
int writen(int fd, char *buff, size_t nbytes);
int readn(int fd, char *buff, size_t nbytes);


int main(void)
{
	int sockfd;
	char buffer[MAXLINE];

	sockfd = open_connection(SERV_PORT, SERV_ADDR);

	if (sockfd < 0)
		exit(1);

	snprintf(buffer, sizeof(buffer), "Hello dude\n");

	if (writen(sockfd, buffer, strlen(buffer)) < 0)
		exit(1);

	if (readn(sockfd, buffer, sizeof(buffer)) < 0)
		exit(1);

	printf("Message: %s\n", buffer);

	return 0;
}

int open_connection(int portnum, char *address)
{
	int sockfd;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "Socket error: %s\n", strerror(errno));
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(portnum);

	if (inet_pton(AF_INET, address, &servaddr.sin_addr) <= 0) {
		fprintf(stderr, "Inet_pton error: %s\n", strerror(errno));
		return -1;
	}

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "connect error: %s\n", strerror(errno));
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
