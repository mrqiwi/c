#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>

#include "functions.h"

int main(void)
{
	char buffer[MAXLINE] = {0};
    char hoststr[INET_ADDRSTRLEN], portstr[INET_ADDRSTRLEN];
	int serverfd, clientfd;
	socklen_t clilen;
    struct sockaddr_storage cliaddr;
    json_error_t jerror;

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
        portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);

        printf("new connection from %s:%s\n", hoststr, portstr);

	    if (readn(clientfd, buffer, sizeof(buffer)) < 0)
	    	exit(1);

        json_auto_t *data = json_loads(buffer, 0, &jerror);

        const char *name = json_string_value(json_object_get(data, "name"));
        int age = json_integer_value(json_object_get(data, "age"));
        bool married = json_boolean_value(json_object_get(data, "married"));

	   	printf("name: %s\n", name);
	   	printf("age: %d\n", age);
	   	printf("married: %s\n", married ? "true" : "false");

	   close(clientfd);
	}

	return 0;
}


