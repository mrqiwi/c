#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BSIZE 1024
#define MAX_REQUEST_SIZE 2047

struct client_info {
    socklen_t address_length;
    struct sockaddr_storage address;
    char address_buffer[128];
    int socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info *next;
};

const char *get_content_type(const char* path)
{
    const char *last_dot = strrchr(path, '.');

    if (!last_dot)
        return "application/octet-stream";

    if (strcmp(last_dot, ".css") == 0)
        return "text/css";
    if (strcmp(last_dot, ".csv") == 0)
        return "text/csv";
    if (strcmp(last_dot, ".gif") == 0)
        return "image/gif";
    if (strcmp(last_dot, ".htm") == 0)
        return "text/html";
    if (strcmp(last_dot, ".html") == 0)
        return "text/html";
    if (strcmp(last_dot, ".ico") == 0)
        return "image/x-icon";
    if (strcmp(last_dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(last_dot, ".jpg") == 0)
        return "image/jpeg";
    if (strcmp(last_dot, ".js") == 0)
        return "application/javascript";
    if (strcmp(last_dot, ".json") == 0)
        return "application/json";
    if (strcmp(last_dot, ".png") == 0)
        return "image/png";
    if (strcmp(last_dot, ".pdf") == 0)
        return "application/pdf";
    if (strcmp(last_dot, ".svg") == 0)
        return "image/svg+xml";
    if (strcmp(last_dot, ".txt") == 0)
        return "text/plain";

    return "application/octet-stream";
}

int create_socket(const char *host, const char *port)
{
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(host, port, &hints, &bind_address);

    printf("Creating socket...\n");
    int socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (socket_listen < 0) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    return socket_listen;
}

struct client_info *get_client(struct client_info **client_list, int s)
{
    struct client_info *ci = *client_list;

    while(ci) {
        if (ci->socket == s)
            break;
        ci = ci->next;
    }

    if (ci)
        return ci;

    struct client_info *n = (struct client_info*) calloc(1, sizeof(struct client_info));

    if (!n) {
        fprintf(stderr, "Out of memory.\n");
        exit(EXIT_FAILURE);
    }

    n->address_length = sizeof(n->address);
    n->next = *client_list;
    *client_list = n;
    return n;
}

void drop_client(struct client_info **client_list, struct client_info *client)
{
    close(client->socket);

    struct client_info **p = client_list;

    while(*p) {
        if (*p == client) {
            *p = client->next;
            free(client);
            return;
        }
        p = &(*p)->next;
    }

    fprintf(stderr, "drop_client not found.\n");
    exit(EXIT_FAILURE);
}

const char *get_client_address(struct client_info *ci)
{
    getnameinfo((struct sockaddr*)&ci->address,
            ci->address_length,
            ci->address_buffer, sizeof(ci->address_buffer), 0, 0,
            NI_NUMERICHOST);

    return ci->address_buffer;
}

fd_set wait_on_clients(struct client_info **client_list, int server)
{
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);
    int max_socket = server;

    struct client_info *ci = *client_list;

    while(ci) {
        FD_SET(ci->socket, &reads);
        if (ci->socket > max_socket)
            max_socket = ci->socket;
        ci = ci->next;
    }

    if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "select() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    return reads;
}


void send_400(struct client_info **client_list, struct client_info *client)
{
    const char *c400 = "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\nBad Request";

    if (send(client->socket, c400, strlen(c400), 0) < 0) {
        fprintf(stderr, "send() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    drop_client(client_list, client);
}

void send_404(struct client_info **client_list, struct client_info *client)
{
    const char *c404 = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\nNot Found";

    if (send(client->socket, c404, strlen(c404), 0) < 0) {
        fprintf(stderr, "send() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    drop_client(client_list, client);
}

void serve_resource(struct client_info **client_list, struct client_info *client, const char *path) {

    printf("serve_resource %s %s\n", get_client_address(client), path);

    if (strcmp(path, "/") == 0)
        path = "/index.html";

    if (strlen(path) > 100) {
        send_400(client_list, client);
        return;
    }

    if (strstr(path, "..")) {
        send_404(client_list, client);
        return;
    }

    char full_path[128];
    sprintf(full_path, "public%s", path);

    FILE *fp = fopen(full_path, "rb");
    if (!fp) {
        send_404(client_list, client);
        return;
    }

    fseek(fp, 0L, SEEK_END);
    size_t cl = ftell(fp);
    rewind(fp);

    const char *ct = get_content_type(full_path);

    char buffer[BSIZE];

    sprintf(buffer, "HTTP/1.1 200 OK\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Connection: close\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Length: %lu\r\n", cl);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", ct);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    int r = fread(buffer, 1, BSIZE, fp);
    while (r) {
        send(client->socket, buffer, r, 0);
        r = fread(buffer, 1, BSIZE, fp);
    }

    fclose(fp);
    drop_client(client_list, client);
}


int main(void)
{
    int server = create_socket(0, "8080");

    struct client_info *client_list = 0;

    while(1) {

        fd_set reads;
        reads = wait_on_clients(&client_list, server);

        if (FD_ISSET(server, &reads)) {
            struct client_info *client = get_client(&client_list, -1);

            client->socket = accept(server, (struct sockaddr*) &(client->address), &(client->address_length));
            if (client->socket < 0) {
                fprintf(stderr, "accept() failed. (%d)\n", errno);
                return 1;
            }

            printf("New connection from %s.\n", get_client_address(client));
        }

        struct client_info *client = client_list;
        while(client) {
            struct client_info *next = client->next;

            if (FD_ISSET(client->socket, &reads)) {

                if (MAX_REQUEST_SIZE == client->received) {
                    send_400(&client_list, client);
                    client = next;
                    continue;
                }

                int r = recv(client->socket, client->request + client->received, MAX_REQUEST_SIZE - client->received, 0);
                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n", get_client_address(client));
                    drop_client(&client_list, client);
                } else {
                    client->received += r;
                    client->request[client->received] = 0;

                    char *q = strstr(client->request, "\r\n\r\n");
                    if (q) {
                        *q = 0;

                        if (strncmp("GET /", client->request, 5)) {
                            send_400(&client_list, client);
                        } else {
                            char *path = client->request + 4;
                            char *end_path = strstr(path, " ");
                            if (!end_path) {
                                send_400(&client_list, client);
                            } else {
                                *end_path = 0;
                                serve_resource(&client_list, client, path);
                            }
                        }
                    } //if (q)
                }
            }

            client = next;
        }

    } //while(1)

    printf("\nClosing socket...\n");
    close(server);

    printf("Finished.\n");
    return 0;
}
