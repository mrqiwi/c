#include <ctype.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>

#include "print.h"

#define MAXCFGLINE 512  /* размер строки конфигурационного файла */
#define MAXKWLEN   16   /* размер ключевого слова */
#define MAXFMTLEN  16   /* размер строки формата */
#define MAXSLEEP   128

int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len)
{
    struct flock lock;

    lock.l_type = type;
    lock.l_start = offset;
    lock.l_whence = whence;
    lock.l_len = len;

    return fcntl(fd, cmd, &lock);
}

int getaddrlist(const char *host, const char *service, struct addrinfo **ailistpp)
{
	int	err;
	struct addrinfo	hint;

	hint.ai_flags = AI_CANONNAME;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = 0;
	hint.ai_addrlen = 0;
	hint.ai_canonname = NULL;
	hint.ai_addr = NULL;
	hint.ai_next = NULL;

	err = getaddrinfo(host, service, &hint, ailistpp);

	return err;
}

static char *scan_configfile(char *keyword)
{
	FILE *fp;
    int	n, match;
	char line[MAXCFGLINE];
	static char	valbuf[MAXCFGLINE];
    char keybuf[MAXKWLEN], pattern[MAXFMTLEN];

    if ((fp = fopen(CONFIG_FILE, "r")) == NULL) {
        syslog(LOG_ERR, "can't open %s: %s", CONFIG_FILE, strerror(errno));
        exit(1);
    }

	sprintf(pattern, "%%%ds %%%ds", MAXKWLEN-1, MAXCFGLINE-1);

	match = 0;
	while (fgets(line, MAXCFGLINE, fp) != NULL) {
		n = sscanf(line, pattern, keybuf, valbuf);

		if (n == 2 && strcmp(keyword, keybuf) == 0) {
			match = 1;
			break;
		}
	}

	fclose(fp);

	if (match != 0)
		return valbuf;
	else
		return NULL;
}

char *get_printserver(void)
{
	return scan_configfile("printserver");
}

struct addrinfo *get_printaddr(void)
{
	int	err;
	char *p;
	struct addrinfo	*ailist;

	if ((p = scan_configfile("printer")) != NULL) {
		if ((err = getaddrlist(p, "ipp", &ailist)) != 0) {
            syslog(LOG_ERR, "no address information for %s", p);
			return NULL;
		}
		return ailist;
	}

    syslog(LOG_ERR, "no printer address specified");
	return NULL;
}

ssize_t tread(int fd, void *buf, size_t nbytes, unsigned int timeout)
{
	int	nfds;
	fd_set readfds;
	struct timeval tv;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	nfds = select(fd+1, &readfds, NULL, NULL, &tv);

	if (nfds <= 0) {
		if (nfds == 0)
			errno = ETIME;
		return -1;
	}

	return read(fd, buf, nbytes);
}

ssize_t treadn(int fd, void *buf, size_t nbytes, unsigned int timeout)
{
	size_t	nleft;
	ssize_t	nread;

	nleft = nbytes;
	while (nleft > 0) {
		if ((nread = tread(fd, buf, nleft, timeout)) < 0) {
			if (nleft == nbytes)
				return -1;
			else
				break;
		} else if (nread == 0) {
			break;
		}
		nleft -= nread;
		buf += nread;
	}
	return nbytes - nleft;
}

ssize_t sendall(int fd, void *ptr, size_t nbytes)
{
	size_t		nleft;
	ssize_t		nwritten;

	nleft = nbytes;
	while (nleft > 0) {
		if ((nwritten = send(fd, ptr, nleft, 0)) < 0) {
			if (nleft == nbytes)
				return -1;
			else
				break;
		} else if (nwritten == 0) {
			break;
		}
		nleft -= nwritten;
		ptr   += nwritten;
	}
	return nbytes - nleft;
}

ssize_t recvall(int fd, void *ptr, size_t nbytes)
{
	size_t	nleft;
	ssize_t	nread;

	nleft = nbytes;
	while (nleft > 0) {
		if ((nread = recv(fd, ptr, nleft, 0)) < 0) {
			if (nleft == nbytes)
				return -1;
			else
				break;
		} else if (nread == 0) {
			break;
		}
		nleft -= nread;
		ptr   += nread;
	}
	return nbytes - nleft;
}

int connect_retry(int domain, int type, int protocol, const struct sockaddr *addr, socklen_t alen)
{
	int fd;

	for (int numsec = 1; numsec <= MAXSLEEP; numsec <<= 1) {

		if ((fd = socket(domain, type, protocol)) < 0)
			return -1;

		if (connect(fd, addr, alen) == 0)
			return fd;

		close(fd);

		if (numsec <= MAXSLEEP/2)
			sleep(numsec);
	}
	return -1;
}
int initserver(int type, const struct sockaddr *addr, socklen_t alen, int qlen)
{
	int fd, err;
	int reuse = 1;

	if ((fd = socket(addr->sa_family, type, 0)) < 0)
		return -1;

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
		goto errout;

	if (bind(fd, addr, alen) < 0)
		goto errout;

	if (type == SOCK_STREAM || type == SOCK_SEQPACKET)
		if (listen(fd, qlen) < 0)
			goto errout;

	return fd;

errout:
	err = errno;
	close(fd);
	errno = err;
	return -1;
}
