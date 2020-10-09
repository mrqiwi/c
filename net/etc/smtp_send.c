#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BSIZE 1024

void get_input(const char *prompt, char *buffer)
{
    printf("%s", prompt);

    fgets(buffer, BSIZE, stdin);

    int len = strlen(buffer);
    if (len > 0)
        buffer[len-1] = 0;
}

void send_format(SSL *ssl, const char *text, ...)
{
    char buffer[BSIZE];
    va_list args;
    va_start(args, text);
    vsprintf(buffer, text, args);
    va_end(args);

    SSL_write(ssl, buffer, strlen(buffer));

    /* printf("C: %s", buffer); */
}

int parse_response(const char *response)
{
    const char *k = response;
    if (!k[0] || !k[1] || !k[2])
        return 0;

    for (; k[3]; ++k) {
        if (k == response || k[-1] == '\n') {
            if (isdigit(k[0]) && isdigit(k[1]) && isdigit(k[2])) {
                if (k[3] != '-') {
                    if (strstr(k, "\r\n")) {
                        return strtol(k, 0, 10);
                    }
                }
            }
        }
    }
    return 0;
}

void wait_on_response(SSL *ssl, int expecting)
{
    char response[BSIZE+1];
    char *p = response;
    char *end = response + BSIZE;

    int code = 0;

    do {
        int bytes_received = SSL_read(ssl, p, end - p);
        if (bytes_received < 1) {
            fprintf(stderr, "Connection dropped.\n");
            exit(EXIT_FAILURE);
        }

        p += bytes_received;
        *p = 0;

        if (p == end) {
            fprintf(stderr, "Server response too large:\n");
            fprintf(stderr, "%s", response);
            exit(EXIT_FAILURE);
        }

        code = parse_response(response);

    } while (code == 0);

    if (code != expecting) {
        fprintf(stderr, "Error from server:\n");
        fprintf(stderr, "%s", response);
        exit(EXIT_FAILURE);
    }

    /* printf("S: %s", response); */
}

int connect_to_host(const char *hostname, const char *port)
{
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;
    if (getaddrinfo(hostname, port, &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
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
    int server = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if (server < 0) {
        fprintf(stderr, "socket() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }

    printf("Connecting...\n");
    if (connect(server, peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", errno);
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(peer_address);

    return server;
}

SSL *get_ssl_socket(SSL_CTX *ctx, int sock)
{
    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        fprintf(stderr, "SSL_new() failed.\n");
        exit(EXIT_FAILURE);
    }

    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) == -1) {
        fprintf(stderr, "SSL_connect() failed.\n");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    /* printf ("SSL/TLS using %s\n", SSL_get_cipher(ssl)); */

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        fprintf(stderr, "SSL_get_peer_certificate() failed.\n");
        exit(EXIT_FAILURE);
    }

    char *tmp;
    if ((tmp = X509_NAME_oneline(X509_get_subject_name(cert),0,0))) {
        /* printf("subject: %s\n", tmp); */
        OPENSSL_free(tmp);
    }

    if ((tmp = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0))) {
        /* printf("issuer: %s\n", tmp); */
        OPENSSL_free(tmp);
    }

    X509_free(cert);

    return ssl;
}
char *base64_encode(char *buffer, size_t length)
{
    BUF_MEM *bufferPtr = NULL;
    if (length <= 0)
        exit(EXIT_FAILURE);

    BIO *b64 = BIO_new(BIO_f_base64());
    if (b64 == NULL)
        exit(EXIT_FAILURE);

    BIO *bio = BIO_new(BIO_s_mem());
    if (bio == NULL) {
        BIO_free_all(b64);
        exit(EXIT_FAILURE);
    }

    bio = BIO_push(b64, bio);

    if (BIO_write(bio, buffer, (int)length) <= 0) {
        BIO_free_all(b64);
        BIO_free_all(bio);
        exit(EXIT_FAILURE);
    }

    if (BIO_flush(bio) != 1) {
        BIO_free_all(b64);
        BIO_free_all(bio);
        exit(EXIT_FAILURE);
    }

    BIO_get_mem_ptr(bio, &bufferPtr);

    char *b64text = (char*) malloc((bufferPtr->length + 1) * sizeof(char));
    if (b64text == NULL) {
        BIO_free_all(b64);
        BIO_free_all(bio);
        exit(EXIT_FAILURE);
    }

    memcpy(b64text, bufferPtr->data, bufferPtr->length);
    b64text[bufferPtr->length - 1] = '\0';

    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(b64);

    return b64text;
}

int main(int argc, char **argv)
{
    if (argc != 2 ) {
        // example: smtp.gmail.com
        fprintf(stderr, "usage: smtp_cli hostname\n");
        return -1;

    }
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        fprintf(stderr, "SSL_CTX_new() failed.\n");
        return -1;
    }

    printf("Connecting to host: %s:465\n", argv[1]);

    int server = connect_to_host(argv[1], "465");
    SSL *ssl = get_ssl_socket(ctx, server);
    wait_on_response(ssl, 220);

    send_format(ssl, "HELO friend\r\n");
    wait_on_response(ssl, 250);

    send_format(ssl, "AUTH LOGIN\r\n");
    wait_on_response(ssl, 334);

    char username[BSIZE];
    get_input("username: ", username);
    char *encoded_data = base64_encode(username, strlen(username));
    send_format(ssl, "%s\r\n", encoded_data);
    free(encoded_data);
    wait_on_response(ssl, 334);

    char *passwd = getpass("password: ");
    encoded_data = base64_encode(passwd, strlen(passwd));
    send_format(ssl, "%s\r\n", encoded_data);
    free(encoded_data);
    wait_on_response(ssl, 235);

    send_format(ssl, "MAIL FROM:<%s>\r\n", username);
    wait_on_response(ssl, 250);

    char recipient[BSIZE];
    get_input("to: ", recipient);
    send_format(ssl, "RCPT TO:<%s>\r\n", recipient);
    wait_on_response(ssl, 250);

    send_format(ssl, "DATA\r\n");
    wait_on_response(ssl, 354);

    char subject[BSIZE];
    get_input("subject: ", subject);

    send_format(ssl, "From:<%s>\r\n", username);
    send_format(ssl, "To:<%s>\r\n", recipient);
    send_format(ssl, "Subject:%s\r\n", subject);

    time_t timer;
    time(&timer);

    struct tm *timeinfo = gmtime(&timer);

    char date[128];
    strftime(date, 128, "%a, %d %b %Y %H:%M:%S +0000", timeinfo);

    send_format(ssl, "Date:%s\r\n", date);
    send_format(ssl, "\r\n");

    printf("Enter your email text, end with \".\" on a line by itself.\n");

    while (1) {
        char body[BSIZE];
        get_input("> ", body);
        send_format(ssl, "%s\r\n", body);
        if (strcmp(body, ".") == 0) {
            break;
        }
    }

    wait_on_response(ssl, 250);

    send_format(ssl, "QUIT\r\n");
    wait_on_response(ssl, 221);
    printf("Email sent\n");

    printf("Closing socket...\n");
    close(server);

    printf("Finished.\n");
    return 0;
}
