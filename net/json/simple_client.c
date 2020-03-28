#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>

#include "functions.h"

int main(void)
{
    int	sockfd = open_connection(SERV_ADDR, SERV_PORT);

	if (sockfd < 0)
		exit(1);

    json_auto_t *jdata = json_pack("{s:s, s:i, s:b}", "name", "Kate", "age", 28, "married", 1);

    char *buffer = json_dumps(jdata, 0);

	if (sendall(sockfd, buffer, strlen(buffer)) < 0)
		exit(1);

	printf("message sent %ld bytes", sizeof buffer);

	return 0;
}

