#ifndef _PRINT_H
#define _PRINT_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define CONFIG_FILE    "/etc/printer.conf"
#define SPOOLDIR       "/var/spool/printer"
#define JOBFILE        "jobno"
#define DATADIR        "data"
#define REQDIR         "reqs"
#define LPNAME		   "lp"

#define FILENMSZ        64
#define FILEPERM        (S_IRUSR|S_IWUSR)
#define USERNM_MAX      64
#define JOBNM_MAX       256
#define MSGLEN_MAX      512

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX   256
#endif

#define IPP_PORT        631     /* default IPP port number */
#define QLEN            10      /* backlog for listen() */

#define IBUFSZ          512	    /* IPP header buffer size */
#define HBUFSZ          512	    /* HTTP header buffer size */
#define IOBUFSZ         8192	/* data buffer size */

#ifndef ETIME
#define ETIME           ETIMEDOUT
#endif

#define	write_lock(fd, offset, whence, len) \
			lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))

//extern
extern int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len);
extern int getaddrlist(const char *, const char *, struct addrinfo **);
extern char *get_printserver(void);
extern struct addrinfo *get_printaddr(void);
extern ssize_t tread(int, void *, size_t, unsigned int);
extern ssize_t treadn(int, void *, size_t, unsigned int);
extern int connect_retry(int, int, int, const struct sockaddr *, socklen_t);
extern int initserver(int, const struct sockaddr *, socklen_t, int);
extern ssize_t sendall(int fd, void *ptr, size_t nbytes);
extern ssize_t recvall(int fd, void *ptr, size_t nbytes);

/*
 * Структура, описывающая запрос утилиты print.
 */
struct printreq {
	uint32_t size;				/* size in bytes */
	uint32_t flags;				/* see below */
	char usernm[USERNM_MAX];	/* user's name */
	char jobnm[JOBNM_MAX];		/* job's name */
};

/*
 * Флаги запроса.
 */
#define PR_TEXT		0x01	    /* интерпретировать файл как обычный текст */

/*
 * Ответ демона на запрос утилиты print.
 */
struct printresp {
	uint32_t retcode;			/* 0=success, !0=error code */
	uint32_t jobid;				/* job ID */
	char msg[MSGLEN_MAX];		/* error message */
};

#endif /* _PRINT_H */
