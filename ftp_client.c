// gcc ftp_client.c -lreadline
// sudo apt-get install libreadline-dev
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/limits.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <readline/readline.h>
#include <readline/history.h>

#define KBYTE 1024
#define NAMEMAX 256
#define CODE 8
#define CTRL_PORT 21

#define GREETING  "ftp>"
#define DELIM     " \t\r\n"

int open_connection(int portnum, char *address);
int init_data_socket(int sockfd);
int sendall(int fd, char *buff, size_t nbytes);
int recvall(int fd, char *buff, size_t nbytes);
int ftp_recv_response(int ctrl_socket);

int login(int sockfd);
int get_filesize(int ctrl_socket, char *filename);
int print_list(int ctrl_socket, char *arg);
int download_file(int ctrl_socket, char *filename);
int upload_file(int ctrl_socket,char *filename);
int delete_file(int ctrl_socket, char *filename);
int make_dir(int ctrl_socket, char *dirname);
int delete_dir(int ctrl_socket, char *dirname);
int change_dir(int ctrl_socket, char *dirname);
int quit(int ctrl_socket, char *arg);

int exec_cmd(int ctrl_socket, char **args);
char **split_line(char *line);

typedef struct {
	char *name;
	int (*func)(int, char*);
} func_t;

func_t funcs_list[] =  {{"put", &upload_file},
						{"get", &download_file},
						{"delete", &delete_file},
						{"rm", &delete_file},
						{"mkdir", &make_dir},
						{"rmdir", &delete_dir},
						{"cd", &change_dir},
						{"dir", &print_list},
						{"ls", &print_list},
						{"exit", &quit},
						{"quit", &quit},
						{NULL, NULL}};


int main(int argc, char **argv)
{
	if (argc == 1) {
		printf("usage: ftp host-name [port]\n");
		return 1;
	}

	char *address = argv[1];
	int portnum = argv[2] ? atoi(argv[2]) : CTRL_PORT;

	int ctrl_socket = open_connection(portnum, address);

	if (ctrl_socket < 0)
		return -1;

	if (login(ctrl_socket) < 0) {
		printf("cannot to log in\n");
		return -1;
	}

	while (1) {
		char *line = readline(GREETING);

		if (line && *line) {
            add_history(line);
			char **args = split_line(line);
			exec_cmd(ctrl_socket, args);
			free(line);
			free(args);
		}
	}

	return 0;
}

char **split_line(char *line)
{
	int bufsize = PATH_MAX;
	int position = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	char *token;

	if (!tokens) {
		printf("malloc() error: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	token = strtok(line, DELIM);

	while (token != NULL) {
		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += PATH_MAX;
			tokens = realloc(tokens, bufsize * sizeof(char*));
			if (!tokens) {
				printf("realloc() error: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, DELIM);
	}
	tokens[position] = NULL;
	return tokens;
}

int exec_cmd(int ctrl_socket, char **args)
{
	func_t *func_obj;

	for (func_obj = funcs_list; func_obj->func; func_obj++)
		if (!strcmp(func_obj->name, args[0]))
			return func_obj->func(ctrl_socket, args[1]);

	printf("no command '%s'\n", args[0]);
	return 0;
}

int open_connection(int portnum, char *address)
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

int print_list(int ctrl_socket, char *arg)
{
	char buffer[BUFSIZ];
	int data_socket, response;

	if ((data_socket = init_data_socket(ctrl_socket)) < 0) {
		printf("init data socket error\n");
		return -1;
	}

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

	if (recvall(data_socket, buffer, sizeof(buffer)) < 0) {
		close(data_socket);
		return -1;
	}

	if (!strlen(buffer))
		printf("directory is empty\n");

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

int get_filesize(int ctrl_socket, char *filename)
{
	int size = -1;
	char buffer[BUFSIZ];

	snprintf(buffer, sizeof(buffer), "SIZE %s\r\n", filename);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	memset(buffer, 0, sizeof(buffer));

	if (recvall(ctrl_socket, buffer, sizeof(buffer)) < 0)
		return -1;

	if (!strlen(buffer)) {
		printf("no data from server\n");
		return -1;
	}

	if (strncmp(buffer, "213", 3)) {
		printf("server response: %s\n", buffer);
		return -1;
	}

	sscanf(buffer, "213 %d", &size);
	return size;
}

int quit(int ctrl_socket, char *arg)
{
	char buffer[BUFSIZ];
	int response, ret = 0;

	snprintf(buffer, sizeof(buffer), "QUIT\r\n");

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		exit(EXIT_FAILURE);

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		exit(EXIT_FAILURE);

	if (response != 221) {
		printf("server response: %d\n", response);
		exit(EXIT_SUCCESS);
	}

	close(ctrl_socket);
	exit(EXIT_SUCCESS);
}

int make_dir(int ctrl_socket, char *dirname)
{
	char buffer[BUFSIZ];
	int response;

	snprintf(buffer, sizeof(buffer), "MKD %s\r\n", dirname);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 257) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}

int delete_dir(int ctrl_socket, char *dirname)
{
	char buffer[BUFSIZ];
	int response;

	snprintf(buffer, sizeof(buffer), "RMD %s\r\n", dirname);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 250) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}

int change_dir(int ctrl_socket, char *dirname)
{
	char buffer[BUFSIZ];
	int response;

	snprintf(buffer, sizeof(buffer), "CWD %s\r\n", dirname);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 250) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}

int delete_file(int ctrl_socket, char *filename)
{
	char buffer[BUFSIZ];
	int response;

	snprintf(buffer, sizeof(buffer), "DELE %s\r\n", filename);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 250) {
		printf("server response: %d\n", response);
		return 1;
	}

	return 0;
}


int upload_file(int ctrl_socket, char *filename)
{
	char buffer[BUFSIZ];
	int data_socket, response;

	if ((data_socket = init_data_socket(ctrl_socket)) < 0) {
		printf("init data socket error\n");
		return -1;
	}

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

	ssize_t nreaded = 0;

	do {
		memset(buffer, 0, sizeof(buffer));
		nreaded = fread(buffer, sizeof(char), sizeof(buffer), fp);
		write(data_socket, buffer, nreaded);

		printf("\ruploading...");
		fflush(stdout);
	} while (nreaded > 0);

	printf("\nfile '%s' uploaded\n", filename);

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

int download_file(int ctrl_socket, char *filename)
{
	char buffer[BUFSIZ];
	int data_socket, filesize, response;

	if ((filesize = get_filesize(ctrl_socket, filename)) < 0)
		return -1;

	if ((data_socket = init_data_socket(ctrl_socket)) < 0) {
		printf("init data socket error\n");
		return -1;
	}

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
		ssize_t nreaded = read(data_socket, buffer, sizeof(buffer) - 1);
		fwrite(buffer, sizeof(char), nreaded, fp);
		nleft -= nreaded;

		printf("\rdownloading...");
		fflush(stdout);
	}

	printf("\nfile '%s' downloaded\n", filename);

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

	if (!strlen(buffer)) {
		printf("no data from server\n");
		return -1;
	}

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

int login(int ctrl_socket)
{
	char buffer[BUFSIZ], username[NAMEMAX];
	int response;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 220) {
		printf("server response: %d\n", response);
		return 1;
	}

	printf("input username: ");
	fgets(username, sizeof(username), stdin);
	snprintf(buffer, sizeof(buffer), "USER %s\r\n", username);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 331) {
		printf("server response: %d\n", response);
		return 1;
	}

    char *pass = getpass("input password: ");
	snprintf(buffer, sizeof(buffer), "PASS %s\r\n", pass);
	free(pass);

	if (sendall(ctrl_socket, buffer, strlen(buffer)) < 0)
		return -1;

	if ((response = ftp_recv_response(ctrl_socket)) < 0)
		return -1;

	if (response != 230) {
		printf("server response: %d\n", response);
		return -1;
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