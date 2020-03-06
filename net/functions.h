#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#define SERV_ADDR "127.0.0.1"
#define SERV_PORT "5000"
#define MAXLINE   1024
#define LISTENQ   1024

int open_connection(char *address, char *portnum);
int open_listen_fd(char *portnum);
int sendall(int fd, char *buff, size_t nbytes);
int readn(int fd, char *buff, size_t nbytes);

#endif
