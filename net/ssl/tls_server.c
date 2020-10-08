// openssl req -x509 -newkey rsa:2048 -nodes -sha256 -keyout key.pem -out cert.pem -days 365
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BSIZE 1024

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: tls_server port\n");
        return -1;
    }

    char *port = argv[1];

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        fprintf(stderr, "SSL_CTX_new() failed.\n");
        return -1;
    }

    if (!SSL_CTX_use_certificate_file(ctx, "cert.pem" , SSL_FILETYPE_PEM)
    || !SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM)) {
        fprintf(stderr, "SSL_CTX_use_certificate_file() failed.\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, port, &hints, &bind_address);

    printf("Creating socket...\n");
    int socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (socket_listen < 0) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        return -1;
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", errno);
        return -1;
    }


    printf("Waiting for connections...\n");

    while(1) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_listen, &reads);

        if (select(socket_listen + 1, &reads, 0, 0, NULL) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", errno);
            return -1;
        }

        struct sockaddr_storage client_address;
        socklen_t client_len = sizeof(client_address);
        int socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_len);
        if (socket_client < 0) {
            fprintf(stderr, "accept() failed. (%d)\n", errno);
            return -1;
        }

        char address_buffer[100];
        getnameinfo((struct sockaddr*)&client_address, client_len,
                    address_buffer, sizeof(address_buffer), 0, 0,
                    NI_NUMERICHOST);

        printf("New connection from %s.\n", address_buffer);

        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            fprintf(stderr, "SSL_new() failed.\n");
            return -1;
        }

        SSL_set_fd(ssl, socket_client);
        if (SSL_accept(ssl) <= 0) {
            fprintf(stderr, "SSL_accept() failed.\n");
            ERR_print_errors_fp(stderr);

            SSL_shutdown(ssl);
            close(socket_client);
            SSL_free(ssl);
            continue;
        }

        printf("SSL connection using %s\n", SSL_get_cipher(ssl));

        printf("Reading request...\n");
        char msg_buff[BSIZE];
        int nbytes = SSL_read(ssl, msg_buff, BSIZE);
        printf("Received %d bytes.\n", nbytes);
        printf("From client: %.*s", nbytes, msg_buff);

        snprintf(msg_buff, BSIZE, "%s", "hello, dude");
        nbytes = SSL_write(ssl, msg_buff, strlen(msg_buff));
        printf("Sent %d of %d bytes.\n", nbytes, (int)strlen(msg_buff));

        SSL_shutdown(ssl);
        close(socket_client);
        SSL_free(ssl);
    }

    printf("Closing listening socket...\n");
    close(socket_listen);
    SSL_CTX_free(ctx);

    return 0;
}
