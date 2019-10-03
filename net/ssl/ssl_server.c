// gcc -o ssl_server ssl_server.c -lssl -lcrypto
// openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout mycert.pem -out mycert.pem
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>  // for socket
#include <arpa/inet.h>   // for htons, htonl
#include <string.h>      // for memset
#include <errno.h>       // for errno
#include <unistd.h>      // for write
#include <openssl/ssl.h>
#include <openssl/err.h>


#define SERV_PORT 5000
#define LISTENQ   1024
#define	MAXLINE	  1024
#define CERTFILE  "mycert.pem"
#define CERTKEY   "mycert.pem"


int open_listen_fd(int portnum);
SSL_CTX *init_server_ctx(void);
int load_certs(SSL_CTX *ctx, char *cert_file, char *key_file);
void show_certs(SSL *ssl);
int ssl_readn(SSL *ssl, char *buff, size_t nbytes);
int ssl_writen(SSL *ssl, char *buff, size_t nbytes);
SSL *ssl_prepare(SSL_CTX* ctx, int fd);


int main(void)
{
	char buffer[MAXLINE];
	int serverfd, clientfd;
	socklen_t clilen;
	struct sockaddr_in cliaddr;
    SSL_CTX *ctx;
    SSL *ssl;

	if ((serverfd = open_listen_fd(SERV_PORT)) < 0)
		exit(1);

    SSL_library_init();

    if ((ctx = init_server_ctx()) == NULL)
    	exit(1);

    if (load_certs(ctx, CERTFILE, CERTKEY) < 0)
    	exit(1);

	for (;;) {
	   	clilen = sizeof(cliaddr);
		clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

		if (clientfd < 0) {
			fprintf(stderr, "Accept error: %s\n", strerror(errno));
			exit(1);
		}

	    printf("Connection from %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

	    if ((ssl = ssl_prepare(ctx, clientfd)) == NULL)
	    	exit(1);

	    // show_certs(ssl);

		if (ssl_readn(ssl, buffer, sizeof(buffer)) < 0)
			exit(1);

	   	printf("Message: %s\n", buffer);

	    snprintf(buffer, sizeof(buffer), "Hello, brother\n");

		if (ssl_writen(ssl, buffer, strlen(buffer)) < 0)
			exit(1);

       SSL_free(ssl);
	   close(clientfd);
	}

    SSL_CTX_free(ctx);
	close(serverfd);

	return 0;
}

int open_listen_fd(int portnum)
{
	int sockfd, enable = 1;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "Socket error: %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		fprintf(stderr, "Setsockopt(SO_REUSEADDR) error: %s\n", strerror(errno));
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(portnum);

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "Bind error: %s\n", strerror(errno));
		return -1;
	}

	if (listen(sockfd, LISTENQ) < 0) {
		fprintf(stderr, "Listen error: %s\n", strerror(errno));
		return -1;
	}

	return sockfd;
}

SSL_CTX* init_server_ctx(void)
{
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ctx = SSL_CTX_new(SSLv23_server_method());

    if (ctx == NULL)
        ERR_print_errors_fp(stderr);

    return ctx;
}

int load_certs(SSL_CTX *ctx, char *cert_file, char *key_file)
{
    if (SSL_CTX_load_verify_locations(ctx, cert_file, key_file) != 1)
        ERR_print_errors_fp(stderr);

    if (SSL_CTX_set_default_verify_paths(ctx) != 1)
        ERR_print_errors_fp(stderr);

    if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        return -1;
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        fprintf(stderr, "Private key does not match the public certificate\n");
        return -1;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
    SSL_CTX_set_verify_depth(ctx, 4);

    return 0;
}

void show_certs(SSL *ssl)
{
    X509 *cert = SSL_get_peer_certificate(ssl);

    if (cert == NULL) {
        printf("No certificates.\n");
        return;
    }

    printf("Server certificates:\n");
    printf("Subject: %s\n", X509_NAME_oneline(X509_get_subject_name(cert), 0, 0));
    printf("Issuer: %s\n", X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0));

    X509_free(cert);
}

int ssl_readn(SSL *ssl, char *buff, size_t nbytes)
{
	size_t nleft;
	ssize_t nreaded;

	nleft = nbytes-1;	/*-1 for '\0'*/

	while (nleft > 0) {
		nreaded = SSL_read(ssl, buff, nleft);

		if (nreaded < 0) {
			ERR_print_errors_fp(stderr);
			return -1;
		} else if ((nreaded == 0) || strstr(buff, "\0"))
			break;

		nleft -= nreaded;
		buff += nreaded;
	}

	return nbytes - nleft;
}

int ssl_writen(SSL *ssl, char *buff, size_t nbytes)
{
	size_t nleft;
	ssize_t nwritten;

	nleft = nbytes+1;	/*+1 for '\0'*/

	while (nleft > 0) {
		if ((nwritten = SSL_write(ssl, buff, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else {
				ERR_print_errors_fp(stderr);
				return -1;
			}
		}

		nleft -= nwritten;
		buff  += nwritten;
	}

	return nbytes;
}

SSL *ssl_prepare(SSL_CTX *ctx, int fd)
{
    SSL *ssl = SSL_new(ctx);

    SSL_set_fd(ssl, fd);

    if (SSL_accept(ssl) == -1) {
    	ERR_print_errors_fp(stderr);
    	return NULL;
    }

    return ssl;
}