#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SERV_ADDR "192.168.0.101"
#define CTRL_PORT 21
#define KBYTE 1024
#define CODE 8

#define USERNAME    "sergey"
#define USERPASSWD  "1"
#define FILENAME    "if.txt"

int open_connection(int portnum, const char *address);
int init_data_socket(int sockfd);
int sendall(int fd, char *buff, size_t nbytes);
int recvall(int fd, char *buff, size_t nbytes);
int ftp_recv_response(int ctrl_socket);

int login(int sockfd, const char *username, const char *userpasswd);
int print_list(int ctrl_socket);
int get_filesize(int ctrl_socket, const char *filename);
int download_file(int ctrl_socket, const char *filename);
int upload_file(int ctrl_socket, const char *filename);
int quit(int ctrl_socket);

//TODO
//		добавить select в recvall
int main(void)
{
	int ctrl_socket, data_socket, filesize;
	int portnum = CTRL_PORT;
	const char *username = USERNAME;
	const char *userpasswd = USERPASSWD;
	const char *filename = "qt.jpg";
	const char *address = SERV_ADDR;

	if ((ctrl_socket = open_connection(portnum, address)) < 0)
		return -1;

	if (login(ctrl_socket, username, userpasswd) < 0)
		return -1;

	if (print_list(ctrl_socket) < 0)
		return -1;

	// if (download_file(ctrl_socket, "if.txt") < 0)
	// 	return -1;

	if (upload_file(ctrl_socket, "if.txt") < 0)
		return -1;

	if (quit(ctrl_socket) < 0)
		return -1;

	return 0;
}

int open_connection(int portnum, const char *address)
{
	int sockfd, enable = 1;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		printf("socket error: %s\n", strerror(errno));
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		printf("setsockopt(SO_REUSEADDR) error: %s\n", strerror(errno));
		return -1;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(portnum);

	if (inet_pton(AF_INET, address, &servaddr.sin_addr) <= 0) {
		printf("inet_pton error: %s\n", strerror(errno));
		return -1;
	}

	if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		printf("connect error: %s\n", strerror(errno));
		return -1;
	}

	return sockfd;
}

int print_list(int ctrl_socket)
{
	char buffer[BUFSIZ];
	int data_socket, response;

	if ((data_socket = init_data_socket(ctrl_socket)) < 0)
		return -1;

	snprintf(buffer, sizeof(buffer), "LIST\r\n");

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 150) {
		printf("server response: %d\n", response);
		return 1;
	}

	memset(buffer, 0, sizeof(buffer));

	if (recvall(data_socket, buffer, sizeof(buffer)) < 0)
		return -1;

	close(data_socket);
	printf("%s", buffer);

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 226) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}

int get_filesize(int ctrl_socket, const char *filename)
{
	int size = -1;
	char buffer[BUFSIZ];

	snprintf(buffer, sizeof(buffer), "SIZE %s\r\n", filename);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	memset(buffer, 0, sizeof(buffer));

	if (recvall(ctrl_socket, buffer, sizeof(buffer)) < 0)
		return -1;

	if (strncmp(buffer, "213", 3)) {
		printf("server response: %s\n", buffer);
		return -1;
	}

	sscanf(buffer, "213 %d", &size);
	return size;
}

int quit(int ctrl_socket)
{
	char buffer[BUFSIZ];
	int response, ret = 0;

	snprintf(buffer, sizeof(buffer), "QUIT\r\n");

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 221) {
		printf("server response: %d\n", response);
		return 1;
	}

	close(ctrl_socket);
	return ret;
}

int upload_file(int ctrl_socket, const char *filename)
{
	char buffer[BUFSIZ];
	int data_socket, response;

	if ((data_socket = init_data_socket(ctrl_socket)) < 0)
		return -1;

	snprintf(buffer, sizeof(buffer), "STOR %s\r\n", filename);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 150) {
		printf("server response: %d\n", response);
		return 1;
	}

	FILE *fp = fopen(filename, "rb");

	if (!fp) {
		printf("fopen error: %s\n", strerror(errno));
		return -1;
	}
//TODO продумать это место, получить размер файла
	int nreaded = 0;
	do {
		memset(buffer, 0, sizeof(buffer));
		nreaded = fread(buffer, sizeof(char), 10, fp);
		write(data_socket, buffer, nreaded);
	} while (nreaded > 0);

//    size_t nleft = filesize;

	// while (nleft > 0) {
	//     memset(buffer, 0, sizeof(buffer));
//        ssize_t nreaded = read(data_socket, buffer, sizeof(buffer));
//        fwrite(buffer, sizeof(char), nreaded, fp);
//        nleft -= nreaded;
	// }

	printf("file '%s' uploaded\n", filename);

	close(data_socket);
	fclose(fp);

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 226) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}

int download_file(int ctrl_socket, const char *filename)
{
	char buffer[BUFSIZ];
	int data_socket, filesize, response;

	if ((filesize = get_filesize(ctrl_socket, filename)) < 0)
		return -1;

	if ((data_socket = init_data_socket(ctrl_socket)) < 0)
		return -1;

	snprintf(buffer, sizeof(buffer), "RETR %s\r\n", filename);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 150) {
		printf("server response: %d\n", response);
		return 1;
	}

	FILE *fp = fopen(filename, "wb");

	if (!fp) {
		printf("fopen error: %s\n", strerror(errno));
		return -1;
	}

	size_t nleft = filesize;

	while (nleft > 0) {
		memset(buffer, 0, sizeof(buffer));
		ssize_t nreaded = read(data_socket, buffer, sizeof(buffer));
		fwrite(buffer, sizeof(char), nreaded, fp);
		nleft -= nreaded;
	}

	printf("file '%s' downloaded\n", filename);

	close(data_socket);
	fclose(fp);

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 226) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}

int init_data_socket(int ctrl_socket)
{
	char buffer[BUFSIZ], address[KBYTE], *data;
	int data_socket;
	int okt1, okt2, okt3, okt4;
	int port, p1, p2;

	snprintf(buffer, sizeof(buffer), "PASV\r\n");

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	memset(buffer, 0, sizeof(buffer));

	if (recvall(ctrl_socket, buffer, sizeof(buffer)) < 0)
		return -1;

	data = strchr(buffer, '(');

	if (sscanf(data, "(%d,%d,%d,%d,%d,%d)", &okt1, &okt2, &okt3, &okt4, &p1, &p2) != 6) {
		printf("incorrect data: %s\n", data);
		return -1;
	}

	snprintf(address, sizeof(address), "%d.%d.%d.%d", okt1, okt2, okt3, okt4);
	port = p1 * 256 + p2;

	if ((data_socket = open_connection(port, address)) < 0)
		return -1;

	return data_socket;
}

int login(int ctrl_socket, const char *username, const char *userpasswd)
{
	char buffer[BUFSIZ];
	int response;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 220) {
		printf("server response: %d\n", response);
		return 1;
	}

	snprintf(buffer, sizeof(buffer), "USER %s\r\n", username);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 331) {
		printf("server response: %d\n", response);
		return 1;
	}

	snprintf(buffer, sizeof(buffer), "PASS %s\r\n", userpasswd);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 230) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}

int ftp_recv_response(int ctrl_socket)
{
	char buffer[BUFSIZ];
	char code[CODE];

	memset(buffer, 0, sizeof(buffer));

	if (recvall(ctrl_socket, buffer, sizeof(buffer)) < 0)
		return -1;

	memset(code, 0, sizeof(code));

	if (sscanf(buffer, "%s", code) != 1)
		return -1;

	return atoi(code);
}

int recvall(int fd, char *buff, size_t nbytes)
{
	size_t nleft;
	ssize_t nreaded;

	nleft = nbytes;

	while (nleft > 0) {
		nreaded = read(fd, buff, nleft);

		if (nreaded < 0) {
			printf("read error: %s\n", strerror(errno));
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
	size_t nleft;
	ssize_t nwritten;

	nleft = nbytes;

	while (nleft > 0) {
		if ((nwritten = write(fd, buff, nleft)) <= 0) {
			if (nwritten < 0 && errno == EINTR)
				nwritten = 0;
			else {
				printf("write error: %s\n", strerror(errno));
				return -1;
			}
		}

		nleft -= nwritten;
		buff  += nwritten;
	}

	return nbytes;
}
