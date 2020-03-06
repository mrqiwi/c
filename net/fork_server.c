#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

#include "functions.h"

static void sig_chld(int signo)
{
	while (waitpid(-1, NULL, WNOHANG) > 0);
	// WNOHANG - немедленное возвращение управления,
    // если ни один дочерний процесс не завершил выполнение
}

int main(void)
{
	char buffer[MAXLINE];
    char hoststr[INET_ADDRSTRLEN], portstr[INET_ADDRSTRLEN];
	int serverfd, clientfd;
	pid_t pid;
	socklen_t clilen;
	struct sockaddr_storage cliaddr;
	struct sigaction act;

	serverfd = open_listen_fd(SERV_PORT);

	if (serverfd < 0)
		exit(1);

	act.sa_handler = sig_chld;
    sigemptyset(&act.sa_mask);
	act.sa_flags |= SA_RESTART;

	if (sigaction(SIGCHLD, &act, NULL) < 0) {
		perror("sigaction error");
		return -1;
	}

	for (;;) {
	   	clilen = sizeof(cliaddr);
		clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

		if (clientfd < 0) {
			if (errno == EINTR)
				continue;
			else {
				perror("accept error");
				exit(1);
			}
		}

        getnameinfo((struct sockaddr *)&cliaddr, clilen, hoststr, sizeof(hoststr),
        portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);

        printf("new connection from %s:%s\n", hoststr, portstr);

		pid = fork();

		if (pid == -1) {
			perror("fork error");
			exit(1);
		}

		if (pid == 0) {
			close(serverfd);

			if (readn(clientfd, buffer, sizeof(buffer)) < 0)
				exit(1);

		   	printf("message: %s\n", buffer);

	    	snprintf(buffer, sizeof(buffer), "Hello, brother");

			if (sendall(clientfd, buffer, strlen(buffer)) < 0)
				exit(1);

			exit(0);
		}
		close(clientfd);
	}

	return 0;
}

