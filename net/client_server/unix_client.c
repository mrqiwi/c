#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "functions.h"

#define SOCKET_NAME "/tmp/9Lq7BNBnBycd6nxy.socket"

int main(int argc, char *argv[])
{
    struct sockaddr_un addr;
    char buffer[BUFSIZE];

    int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_NAME, sizeof(addr.sun_path) - 1);

    int res = connect(sock, (const struct sockaddr *) &addr, sizeof(struct sockaddr_un));
    if (res < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; ++i) {
        res = write(sock, argv[i], strlen(argv[i]) + 1);
        if (res < 0) {
            perror("write");
            break;
        }
    }

    strcpy(buffer, "END");
    res = write(sock, buffer, strlen(buffer) + 1);
    if (res < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }

    res = read(sock, buffer, BUFSIZE);
    if (res < 0) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    buffer[BUFSIZE - 1] = 0;

    printf("Result = %s\n", buffer);

    close(sock);
    exit(EXIT_SUCCESS);
}
