// gcc -o ssl_client ssl_client.c -lssl -lcrypto
// openssl req -x509 -nodes -days 365 -newkey rsa:1024 -keyout mycert.pem -out mycert.pem
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h> // for socket
#include <arpa/inet.h>  // for htons, htonl
#include <string.h>		// for memset
#include <errno.h>		// for errno
#include <unistd.h>		// for write
#include <openssl/ssl.h>
#include <openssl/err.h>


#define SERV_ADDR "127.0.0.1"
#define SERV_PORT 5000
#define	MAXLINE	  1024
#define CERTFILE  "mycert.pem"
#define CERTKEY   "mycert.pem"


SSL_CTX *init_client_ctx(void);
int open_connection(int portnum, char *address);
int load_certs(SSL_CTX *ctx, char *cert_file, char *key_file);
SSL *ssl_prepare(SSL_CTX *ctx, int fd);
void show_certs(SSL *ssl);
int ssl_readn(SSL *ssl, char *buff, size_t nbytes);
int ssl_writen(SSL *ssl, char *buff, size_t nbytes);


int main(void)
{
	int sockfd;
	char buffer[MAXLINE];
	SSL_CTX *ctx;
    SSL *ssl;

	if ((sockfd = open_connection(SERV_PORT, SERV_ADDR)) < 0)
		exit(1);

    SSL_library_init();

    if ((ctx = init_client_ctx()) == NULL)
    	exit(1);

    if (load_certs(ctx, CERTFILE, CERTKEY) < 0)
    	exit(1);

    if ((ssl = ssl_prepare(ctx, sockfd)) == NULL)
    	exit(1);

    printf("Connected with %s encryption\n", SSL_get_cipher(ssl));

    // show_certs(ssl);

	snprintf(buffer, sizeof(buffer), "Hello dude\n");

	if (ssl_writen(ssl, buffer, strlen(buffer)) < 0)
		exit(1);

	if (ssl_readn(ssl, buffer, sizeof(buffer)) < 0)
		exit(1);

	printf("Message: %s\n", buffer);

    SSL_free(ssl);
	SSL_CTX_free(ctx);
	close(sockfd);

	return 0;
}

SSL_CTX *init_client_ctx(void)
{
    SSL_CTX *ctx;

    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ctx = SSL_CTX_new(SSLv23_client_method());

    if (ctx == NULL)
        ERR_print_errors_fp(stderr);

    return ctx;
}

int open_connection(int portnum, char *address)
{
	int sockfd;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		fprintf(stderr, "Socket error: %s\n", strerror(errno));
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(portnum);

	if (inet_pton(AF_INET, address, &servaddr.sin_addr) <= 0) {
		fprintf(stderr, "Inet_pton error: %s\n", strerror(errno));
		return -1;
	}

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		fprintf(stderr, "Connect error: %s\n", strerror(errno));
		return -1;
	}

	return sockfd;
}

int load_certs(SSL_CTX *ctx, char *cert_file, char *key_file)
{
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

    return 0;
}

SSL *ssl_prepare(SSL_CTX *ctx, int fd)
{
	SSL *ssl = SSL_new(ctx);

    SSL_set_fd(ssl, fd);

    if (SSL_connect(ssl) == -1) {
        ERR_print_errors_fp(stderr);
        return NULL;
    }

    return ssl;
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