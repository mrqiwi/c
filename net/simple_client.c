//клиент: nc localhost 5000
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h> // for socket
#include <arpa/inet.h>  // for htons, htonl
#include <string.h>		// for memset
#include <errno.h>		// for errno
#include <unistd.h>		// for write


#define SERV_ADDR "127.0.0.1"
#define SERV_PORT 5000
#define	MAXLINE	  1024


int main(void)
{
	int sockfd;
	struct sockaddr_in servaddr;
	char sendbuff[MAXLINE], recbuff[MAXLINE];
	size_t nleft;
	ssize_t nwritten, nreaded;
	char *pbuff;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "Socket error: %s\n", strerror(errno));
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(SERV_PORT);

	if (inet_pton(AF_INET, SERV_ADDR, &servaddr.sin_addr) <= 0) {
		fprintf(stderr, "Inet_pton error: %s\n", strerror(errno));
		return -1;
	}

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "connect error: %s\n", strerror(errno));
		return -1;
	}

	snprintf(sendbuff, sizeof(sendbuff), "Hello dude");

	nleft = strlen(sendbuff)+1; /*+1 for '\0'*/
	pbuff = sendbuff;

	while (nleft > 0) {
		if ((nwritten = write(sockfd, pbuff, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else {
				fprintf(stderr, "Write error: %s\n", strerror(errno));
				return -1;
			}
		}

		nleft -= nwritten;
		pbuff  += nwritten;
	}

	nleft = sizeof(recbuff);
	pbuff = recbuff;

	while (nleft > 0) {
		nreaded = read(sockfd, pbuff, MAXLINE-1); /*-1 for '\0'*/

		if (nreaded < 0) {
			fprintf(stderr, "Read error: %s\n", strerror(errno));
			return -1;
		} else if ((nreaded == 0) || strstr(pbuff, "\0"))
			break;

		pbuff += nreaded;
		nleft -= nreaded;
	}

	printf("Message: %s\n", recbuff);

	return 0;
}
