//клиент: nc localhost 5000
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>	// for wait
#include <sys/socket.h> // for socket
#include <arpa/inet.h>  // for htons, htonl
#include <string.h>		// for memset
#include <errno.h>		// for errno
#include <unistd.h>		// for write
#include <signal.h>		// for signal

#define SERV_PORT 5000
#define LISTENQ   1024
#define	MAXLINE	  1024


void sig_chld(int signo)
{
	pid_t pid;
	int stat;

	pid = wait(&stat);

  	return;
}

int main(void)
{
	char sendbuff[MAXLINE], recbuff[MAXLINE];
	int serverfd, clientfd, enable = 1;
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;
	pid_t pid;
	size_t nleft;
	ssize_t nwritten, nreaded;
	char *pbuff;
	struct sigaction act;

	serverfd = socket(AF_INET, SOCK_STREAM, 0);

	if (serverfd < 0) {
		fprintf(stderr, "Socket error: %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		fprintf(stderr, "Setsockopt(SO_REUSEADDR) error: %s\n", strerror(errno));
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);

	if (bind(serverfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "Bind error: %s\n", strerror(errno));
		return -1;
	}

	if (listen(serverfd, LISTENQ) < 0) {
		fprintf(stderr, "Listen error: %s\n", strerror(errno));
		return -1;
	}

	// для ожидания дочерних процессов
	act.sa_handler = sig_chld;
	act.sa_flags |= SA_RESTART;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGCHLD, &act, 0) < 0) {
		fprintf(stderr, "Sigaction error: %s\n", strerror(errno));
		return -1;
	}

	for (;;) {
	   	clilen = sizeof(cliaddr);
		clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

		if (clientfd < 0) {
			fprintf(stderr, "Accept error: %s\n", strerror(errno));
			return -1;
		}

	    printf("Connection from %s:%d\n", inet_ntop(AF_INET, &cliaddr.sin_addr, sendbuff, sizeof(sendbuff)), ntohs(cliaddr.sin_port));

		pid = fork();

		if (pid  == -1) {
			fprintf(stderr, "Fork error: %s\n", strerror(errno));
			return -1;
		}

		if (pid == 0) {
			close(serverfd);

			nleft = sizeof(recbuff)-1;
			pbuff = recbuff;

			while (nleft > 0) {
				nreaded = read(clientfd, pbuff, MAXLINE-1); /*-1 for '\0'*/

				if (nreaded < 0) {
					fprintf(stderr, "Read error: %s\n", strerror(errno));
					return -1;
				} else if ((nreaded == 0) || strstr(pbuff, "\0"))
					break;

				pbuff += nreaded;
				nleft -= nreaded;
			}

		   	printf("Message: %s\n", recbuff);

	    	snprintf(sendbuff, sizeof(sendbuff), "Hello, brother");

			nleft = strlen(sendbuff)+1; /*+1 for '\0'*/
			pbuff = sendbuff;

			while (nleft > 0) {
				if ((nwritten = write(clientfd, pbuff, nleft)) <= 0) {
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

			return 0;
		}
		close(clientfd);
	}

	return 0;
}
