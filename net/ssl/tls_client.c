#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BSIZE 1024

int main(int argc, char *argv[])
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    /* SSL_CTX *ctx = SSL_CTX_new(TLS_client_method()); */
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        fprintf(stderr, "SSL_CTX_new() failed.\n");
        return -1;
    }

    if (argc < 3) {
        fprintf(stderr, "usage: tls_client hostname port\n");
        return -1;
    }

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;
    if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", errno);
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

    printf("Creating socket...\n");
    int socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if (socket_peer < 0) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        return -1;
    }

    printf("Connecting...\n");
    if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", errno);
        return -1;
    }
    freeaddrinfo(peer_address);

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        fprintf(stderr, "SSL_new() failed.\n");
        return -1;
    }

    SSL_set_fd(ssl, socket_peer);
    if (SSL_connect(ssl) == -1) {
        fprintf(stderr, "SSL_connect() failed.\n");
        ERR_print_errors_fp(stderr);
        return -1;
    }

    printf ("SSL/TLS using %s\n", SSL_get_cipher(ssl));

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        fprintf(stderr, "SSL_get_peer_certificate() failed.\n");
        return -1;
    }

    char *tmp;
    if ((tmp = X509_NAME_oneline(X509_get_subject_name(cert),0,0))) {
        printf("subject: %s\n", tmp);
        OPENSSL_free(tmp);
    }

    if ((tmp = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0))) {
        printf("issuer: %s\n", tmp);
        OPENSSL_free(tmp);
    }

    X509_free(cert);

    printf("Connected.\n");
    printf("To send data, enter text followed by enter.\n");

    char msg_buff[BSIZE];
    fgets(msg_buff, BSIZE, stdin);
    printf("Sending: %s", msg_buff);
    int nbytes = SSL_write(ssl, msg_buff, strlen(msg_buff));
    printf("Sent %d bytes.\n", nbytes);

    nbytes = SSL_read(ssl, msg_buff, BSIZE);
    printf("Received (%d bytes): %.*s\n", nbytes, nbytes, msg_buff);

    printf("Closing socket...\n");
    SSL_shutdown(ssl);
    close(socket_peer);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    printf("Finished.\n");
    return 0;
}

