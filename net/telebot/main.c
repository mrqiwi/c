// 1) openssl req -newkey rsa:2048 -sha256 -nodes -x509 -days 365 \
-keyout YOURPRIVATE.key \
-out YOURPUBLIC.crt \
-subj "/C=RU/ST=Saint-Petersburg/L=Saint-Petersburg/O=Example Inc/CN=<IP-address>"
// 2) openssl x509 -in YOURPUBLIC.crt -out YOURPUBLIC.pem -outform PEM
// 3) curl -F "url=https://<IP-address>:8443/<TOKEN>" -F "certificate=@YOURPUBLIC.pem" https://api.telegram.org/bot<TOKEN>/setWebhook
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

static int init_server_socket(const char *port)
{
    puts("Configuring local address...");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    if (getaddrinfo(0, port, &hints, &bind_address)) {
        perror("getaddrinfo() failed");
        return -1;
    }
    puts("Creating socket...");
    int socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (socket_listen < 0) {
        perror("socket() failed");
        freeaddrinfo(bind_address);
        return -1;
    }

    puts("Binding socket to local address...");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        puts("bind() failed");
        freeaddrinfo(bind_address);
        return -1;
    }

    freeaddrinfo(bind_address);
    return socket_listen;
}

static int init_client_socket(const char *host, const char *port)
{
    puts("Configuring remote address...");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *peer_address;
    if (getaddrinfo(host, port, &hints, &peer_address)) {
        perror("getaddrinfo() failed");
        return -1;
    }

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
            address_buffer, sizeof(address_buffer),
            service_buffer, sizeof(service_buffer),
            NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);

    puts("Creating socket...");
    int socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if (socket_peer < 0) {
        perror("socket() failed");
        return -1;
    }

    puts("Connecting...");
    if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
        perror("connect() failed");
        return -1;
    }

    freeaddrinfo(peer_address);
    return socket_peer;
}

int main(int argc, char **argv)
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    /* SSL_CTX *ctx = SSL_CTX_new(TLS_server_method()); */
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
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


    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", errno);
        return -1;
    }

    printf("Waiting for connections...\n");
    while(1) {
        struct sockaddr_storage client_address;
        socklen_t client_len = sizeof(client_address);
        int socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_len);
        if (socket_client < 0) {
            fprintf(stderr, "accept() failed. (%d)\n", errno);
            return 1;
        }

        char address_buffer[100];
        getnameinfo((struct sockaddr*)&client_address,
                client_len,
                address_buffer, sizeof(address_buffer), 0, 0,
                NI_NUMERICHOST);
        printf("New connection from %s\n", address_buffer);

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
        if (nbytes < 1) {
            close(socket_client);
            continue;
        }
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
