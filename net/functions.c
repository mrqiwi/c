#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#include "functions.h"

int open_connection(char *address, char *portnum)
{
    int sockfd, res;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((res = getaddrinfo(address, portnum, &hints, &servinfo)) != 0) {
        printf("getaddrinfo error: %s\n", gai_strerror(res));
        return -1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (sockfd < 0) {
        perror("socket error");
        return -1;
    }

    if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("connect error");
        return -1;
    }

    freeaddrinfo(servinfo);

    return sockfd;
}

int open_listen_fd(char *portnum)
{
    int res, sockfd, enable = 1;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((res = getaddrinfo(NULL, portnum, &hints, &servinfo)) != 0) {
        printf("getaddrinfo error: %s\n", gai_strerror(res));
        return -1;
    }

    sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (sockfd < 0) {
        perror("socket error");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) error");
        return -1;
    }

    if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind error");
        return -1;
    }

    if (listen(sockfd, LISTENQ) < 0) {
        perror("listen error");
        return -1;
    }

    freeaddrinfo(servinfo);

    return sockfd;
}

int readn(int fd, char *buff, size_t nbytes)
{
    size_t nleft;
    ssize_t nreaded;

    memset(buff, 0, nbytes);
    nleft = nbytes-1;   /*-1 for '\0'*/

    while (nleft > 0) {
        nreaded = read(fd, buff, nleft);

        if (nreaded < 0) {
            perror("read error");
            return -1;
        } else if ((nreaded == 0) || strstr(buff, "\0"))
            break;

        nleft -= nreaded;
        buff += nreaded;
    }

    return nbytes - nleft;
}

int sendall(int fd, char *buff, size_t nbytes)
{
    ssize_t sent;

    while (nbytes > 0) {
        if ((sent = send(fd, buff, nbytes, 0)) <= 0) {
            if (sent < 0 && errno == EINTR)
                sent = 0;
            else {
                perror("send error");
                return -1;
            }
        }
        nbytes -= sent;
        buff  += sent;
    }

    return 0;
}

