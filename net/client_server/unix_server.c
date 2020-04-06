#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "functions.h"

#define SOCKET_NAME "/tmp/9Lq7BNBnBycd6nxy.socket"

int main(void)
{
    struct sockaddr_un name;
    int down_flag = 0;
    char buffer[BUFSIZE];

    unlink(SOCKET_NAME);

    int server_sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&name, 0, sizeof(struct sockaddr_un));

    name.sun_family = AF_UNIX;
    strncpy(name.sun_path, SOCKET_NAME, sizeof(name.sun_path) - 1);

    int res = bind(server_sock, (const struct sockaddr *) &name, sizeof(struct sockaddr_un));
    if (res < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    res = listen(server_sock, 20);
    if (res < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        int result = 0;
        for(;;) {
            res = read(client_sock, buffer, BUFSIZE);
            if (res < 0) {
                perror("read");
                exit(EXIT_FAILURE);
            }

            buffer[BUFSIZE - 1] = 0;

            if (strncmp(buffer, "DOWN", BUFSIZE) == 0) {
                down_flag = 1;
                break;
            }

            if (strncmp(buffer, "END", BUFSIZE) == 0)
                break;

            result += atoi(buffer);
        }

        sprintf(buffer, "%d", result);
        res = write(client_sock, buffer, BUFSIZE);

        if (res < 0) {
            perror("write");
            exit(EXIT_FAILURE);
        }

        close(client_sock);

        if (down_flag)
            break;
    }

    close(server_sock);
    unlink(SOCKET_NAME);

    exit(EXIT_SUCCESS);
}

