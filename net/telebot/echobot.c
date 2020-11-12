// 1) openssl req -newkey rsa:2048 -sha256 -nodes -x509 -days 365 \
-keyout YOURPRIVATE.key \
-out YOURPUBLIC.crt \
-subj "/C=RU/ST=Saint-Petersburg/L=Saint-Petersburg/O=Example Inc/CN=<IP-address>"
// 2) openssl x509 -in YOURPUBLIC.crt -out YOURPUBLIC.pem -outform PEM
// 3) curl -F "url=https://<IP-address>:8443/<TOKEN>" -F "certificate=@YOURPUBLIC.pem" https://api.telegram.org/bot<TOKEN>/setWebhook
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <jansson.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BSIZE 1024*10
#define MSGSIZE 4096
#define PRIVATEKEY "YOURPRIVATE.key"
#define CERTIFICATE "YOURPUBLIC.crt"
#define LISTENPORT "8443"
#define SENDPORT "443"
#define HOST "api.telegram.org"
#define REQUEST_PATTERN "POST /bot%s/sendMessage HTTP/1.1\r\n" \
                        "Host: %s\r\n" \
                        "Content-Type: application/json\r\n" \
                        "Content-Length: %d\r\n" \
                        "Connection: close\r\n\r\n%s"

typedef struct {
    char text[MSGSIZE];
    int date;
    int chat_id;
} message_t;

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

    if (setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
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

    puts("Listening...");
    if (listen(socket_listen, 10) < 0) {
        puts("listen() failed");
        return -1;
    }

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
        freeaddrinfo(peer_address);
        return -1;
    }

    puts("Connecting...");
    if (connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
        perror("connect() failed");
        freeaddrinfo(peer_address);
        return -1;
    }

    freeaddrinfo(peer_address);
    return socket_peer;
}

static int parse_msg_data(const char *request, message_t *msg)
{
    char *text = NULL;
    json_error_t error;

    if (!request || !*request)
        return -1;

    char *ptr = strchr(request, '{');
    if (!ptr)
        return -1;

    json_t *data = json_loads(ptr, JSON_DECODE_ANY, &error);
    if (!data) {
        printf("json_loads() failed: %d:%d: %s\n", error.line, error.column, error.text);
        return -1;
    }

    if (json_unpack_ex(data, &error, 0, "{s:{s:i,s:s,s:{s:i}}}",
                                        "message",
                                        "date", &msg->date,
                                        "text", &text,
                                        "chat", "id", &msg->chat_id) < 0) {
        printf("json_unpack_ex() failed: %d:%d: %s\n", error.line, error.column, error.text);
        json_decref(data);
        return -1;
    }

    snprintf(msg->text, MSGSIZE, "%s", text ? text: "");
    json_decref(data);
    return 0;
}

ssize_t recvall(SSL *ssl, void *buffer, size_t n)
{
    ssize_t readed;
    size_t total;
    char *buf;

    buf = buffer;
    for (total = 0; total < n; ) {
        readed = SSL_read(ssl, buf, n - total);

        if (readed == 0)
            return total;

        if (readed == -1) {
            if (errno == EINTR)
                continue;
            else
                return -1;
        }
        total += readed;
        buf += readed;
    }
    return total;
}

static int send_message(int chat_id, char *text)
{
    int data_len = snprintf(NULL, 0, "{\"chat_id\":%d,\"text\":\"%s\"}", chat_id, text) + 1;
    char data_buf[data_len];
    snprintf(data_buf, data_len, "{\"chat_id\":%d,\"text\":\"%s\"}", chat_id, text);

    int msg_len = snprintf(NULL, 0, REQUEST_PATTERN, getenv("TOKEN"), HOST, (int)strlen(data_buf), data_buf) + 1;
    char msg_buf[msg_len];
    snprintf(msg_buf, msg_len, REQUEST_PATTERN, getenv("TOKEN"), HOST, (int)strlen(data_buf), data_buf);

    int sock = init_client_socket(HOST, SENDPORT);
    if (sock < 0) {
        puts("Cannot init socket");
        return -1;
    }

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    /* SSL_CTX *ctx = SSL_CTX_new(TLS_client_method()); */
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        puts("SSL_CTX_new() failed");
        close(sock);
        return -1;
    }

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        puts("SSL_new() failed");
        close(sock);
        SSL_CTX_free(ctx);
        return -1;
    }

    SSL_set_fd(ssl, sock);
    if (SSL_connect(ssl) == -1) {
        puts("SSL_connect() failed");
        ERR_print_errors_fp(stdout);
        SSL_shutdown(ssl);
        close(sock);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return -1;
    }

    int nbytes = SSL_write(ssl, msg_buf, strlen(msg_buf));
    if (nbytes < 0) {
        puts("SSL_write() failded");
        SSL_shutdown(ssl);
        close(sock);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return -1;
    }

    printf("Sent %d of %d bytes:\n %s\n\n", nbytes, (int)strlen(msg_buf), msg_buf);

    SSL_shutdown(ssl);
    close(sock);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    return 0;
}


int main(void)
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    /* SSL_CTX *ctx = SSL_CTX_new(TLS_server_method()); */
    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    if (!ctx) {
        puts("SSL_CTX_new() failed");
        return -1;
    }

    if (!SSL_CTX_use_certificate_file(ctx, CERTIFICATE, SSL_FILETYPE_PEM)
        || !SSL_CTX_use_PrivateKey_file(ctx, PRIVATEKEY, SSL_FILETYPE_PEM)) {
        puts("SSL_CTX_use_certificate_file() failed");
        ERR_print_errors_fp(stdout);
        return -1;
    }

    int socket_listen = init_server_socket(LISTENPORT);
    if (socket_listen < 0) {
        puts("Cannot init listen socket");
        return -1;
    }

    puts("Waiting for connections...");
    while (1) {
        struct sockaddr_storage client_address;
        socklen_t client_len = sizeof(client_address);
        int socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_len);
        if (socket_client < 0) {
            puts("accept() failed");
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
            puts("SSL_new() failed");
            close(socket_client);
            return -1;
        }

        SSL_set_fd(ssl, socket_client);
        if (SSL_accept(ssl) <= 0) {
            puts("SSL_accept() failed");
            ERR_print_errors_fp(stdout);
            close(socket_client);
            SSL_free(ssl);
            continue;
        }

        printf("SSL connection using %s\n", SSL_get_cipher(ssl));

        puts("Reading request...");
        char request[BSIZE];
        int nbytes = recvall(ssl, request, BSIZE);
        if (nbytes < 0) {
            close(socket_client);
            SSL_free(ssl);
            continue;
        }

        request[nbytes] = '\0';
        printf("Received %d bytes: %s\n\n", nbytes, request);

        message_t msg;
        if (parse_msg_data(request, &msg) < 0) {
            puts("Cannot parse data from request");
            close(socket_client);
            SSL_free(ssl);
            return -1;
        }
        //NOTE сервер не понимает, что мы получили сообщение
        char *response = "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n";
        nbytes = SSL_write(ssl, response, strlen(response));
        if (nbytes < 0) {
            close(socket_client);
            SSL_free(ssl);
            continue;
        }

        printf("Sent %d of %d bytes: %s\n\n", nbytes, (int)strlen(response), response);

        if (send_message(msg.chat_id, msg.text) < 0) {
            puts("Cannot send message");
            close(socket_client);
            SSL_free(ssl);
            continue;
        }

        close(socket_client);
        SSL_free(ssl);
    }

    puts("Closing listening socket...\n");
    close(socket_listen);
    SSL_CTX_free(ctx);

    return 0;
}
