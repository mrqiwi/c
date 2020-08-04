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

#define BUFSIZE 4096

typedef struct {
    int (*open_conn)(char *, char *);
    ssize_t (*send_data)(int, const void *, size_t);
    ssize_t (*recv_data)(int, void *, size_t);
} conn_t;

static int open_connection(char *address, char *portnum)
{
    int sock, res;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((res = getaddrinfo(address, portnum, &hints, &servinfo)) != 0) {
        printf("getaddrinfo() error: %s\n", gai_strerror(res));
        return -1;
    }

    sock = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (sock < 0) {
        perror("socket() error");
        return -1;
    }

    if (connect(sock, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("connect() error");
        return -1;
    }

    freeaddrinfo(servinfo);

    return sock;
}

void init_conn(conn_t *conn)
{
    conn->open_conn = &open_connection;
    conn->send_data = &write;
    conn->recv_data = &read;
}

int main(int argc, char *argv[])
{
    conn_t conn;
    char buffer[BUFSIZE] = {0};

    if (argc != 3) {
        puts("usage: program <address> <portnum>");
        return -1;
    }

    init_conn(&conn);

    int sock = conn.open_conn(argv[1], argv[2]);
	if (sock < 0)
		return -1;

    int len = snprintf(buffer, sizeof(buffer), "GET / HTTP/1.1\r\nHost: %s\r\nContent-Type: text/plain\r\n\r\n", argv[1]);

    printf("Request: \n%s", buffer);

    int res = conn.send_data(sock, buffer, len);
    if (res < 0) {
        perror("send_data() error");
        return -1;
    }

    res = conn.recv_data(sock, buffer, sizeof(buffer));
    if (res < 0) {
        perror("recv_data() error");
        return -1;
    }

    printf("Response: \n%s", buffer);

    return 0;
}
