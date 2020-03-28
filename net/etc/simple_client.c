#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "functions.h"

int main(void)
{
	int sockfd;
	char buffer[MAXLINE];

	sockfd = open_connection(SERV_ADDR, SERV_PORT);

	if (sockfd < 0)
		exit(1);

	snprintf(buffer, sizeof(buffer), "Hello dude");

	if (sendall(sockfd, buffer, strlen(buffer)) < 0)
		exit(1);

	if (readn(sockfd, buffer, sizeof(buffer)) < 0)
		exit(1);

	printf("message: %s\n", buffer);

	return 0;
}

