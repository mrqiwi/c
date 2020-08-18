#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>
#include <pwd.h>
#include <pthread.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>

#include "print.h"
#include "ipp.h"

/*
 * Ответы принтера по протоколу HTTP.
 */
#define HTTP_INFO(x)	((x) >= 100 && (x) <= 199)
#define HTTP_SUCCESS(x) ((x) >= 200 && (x) <= 299)

/*
 * Описание заданий для печати
 */
struct job {
	struct job *next;
	struct job *prev;
	int32_t jobid;
	struct printreq req;
};

/*
 * Описание потока, обрабатывающего запрос от клиента.
 */
struct worker_thread {
	struct worker_thread *next;
	struct worker_thread *prev;
	pthread_t  tid;
	int sockfd;
};

/*
 * Для журналирования
 */
int	log_to_stderr = 0;

/*
 * Переменные, имеющие отношение к принтеру
 */
struct addrinfo	*printer;
char *printer_name;
pthread_mutex_t	configlock = PTHREAD_MUTEX_INITIALIZER;
int	reread;

/*
 * Переменные, имеющие отношение к потоку
 */
struct worker_thread *workers;
pthread_mutex_t	workerlock = PTHREAD_MUTEX_INITIALIZER;
sigset_t mask;

/*
 * Переменные, имеющие отношение к заданиям
 */
struct job	*jobhead, *jobtail;
int	jobfd;
int32_t	nextjob;
pthread_mutex_t	joblock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t	jobwait = PTHREAD_COND_INITIALIZER;

void init_request(void);
void init_printer(void);
void update_jobno(void);
int32_t	get_newjobno(void);
void add_job(struct printreq *, int32_t);
void replace_job(struct job *);
void remove_job(struct job *);
void build_qonstart(void);
void *client_thread(void *);
void *printer_thread(void *);
void *signal_thread(void *);
ssize_t	readmore(int, char **, int, int *);
int	printer_status(int, struct job *);
void add_worker(pthread_t, int);
void kill_workers(void);
void client_cleanup(void *);

/*
 * Главный поток сервера печати. Принимает запросы на соединение
 * от клиентов и запускает дополнительные потоки для обработки запросов.
 */
int main(int argc, char *argv[])
{
	char *host;
    pthread_t tid;
	struct sigaction sa;
	struct passwd *pwdp;
    fd_set rendezvous, rset;
    int	sockfd, err, i, n, maxfd;
    struct addrinfo	*ailist, *aip;

    if (argc != 1) {
        puts("usage: printd");
        return -1;
    }

    daemon(0, 0);

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sa.sa_handler = SIG_IGN;

    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        syslog(LOG_ERR, "sigaction failed");
        return -1;
    }

	sigemptyset(&mask);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGTERM);

    if ((err = pthread_sigmask(SIG_BLOCK, &mask, NULL)) != 0) {
        syslog(LOG_ERR, "pthread_sigmask failed");
        return -1;
    }

	n = sysconf(_SC_HOST_NAME_MAX);

	if (n < 0)
		n = HOST_NAME_MAX;

    if ((host = malloc(n)) == NULL) {
        syslog(LOG_ERR, "malloc error");
        return -1;
    }

    if (gethostname(host, n) < 0) {
        syslog(LOG_ERR, "gethostname error");
        return -1;
    }

	if ((err = getaddrlist(host, "print", &ailist)) != 0) {
		syslog(LOG_ERR, "getaddrinfo error: %s", gai_strerror(err));
		return -1;
	}

	FD_ZERO(&rendezvous);
	maxfd = -1;

	for (aip = ailist; aip != NULL; aip = aip->ai_next) {
		if ((sockfd = initserver(SOCK_STREAM, aip->ai_addr, aip->ai_addrlen, QLEN)) >= 0) {
			FD_SET(sockfd, &rendezvous);

			if (sockfd > maxfd)
				maxfd = sockfd;
		}
	}

    if (maxfd == -1) {
        syslog(LOG_ERR, "service not enabled");
        return -1;
    }

	pwdp = getpwnam(LPNAME);

    if (pwdp == NULL) {
        syslog(LOG_ERR, "can't find user %s", LPNAME);
        return -1;
    }

    if (pwdp->pw_uid == 0) {
        syslog(LOG_ERR, "user %s is privileged", LPNAME);
        return -1;
    }

    if (setgid(pwdp->pw_gid) < 0 || setuid(pwdp->pw_uid) < 0) {
        syslog(LOG_ERR, "can't change IDs to user %s", LPNAME);
        return -1;
    }

	init_request();
	init_printer();

	err = pthread_create(&tid, NULL, printer_thread, NULL);

	if (err == 0)
		err = pthread_create(&tid, NULL, signal_thread, NULL);

    if (err != 0) {
        syslog(err, "can't create thread");
        return -1;
    }

	build_qonstart();

	syslog(LOG_INFO, "daemon initialized");

	for (;;) {
		rset = rendezvous;

        if (select(maxfd+1, &rset, NULL, NULL, NULL) < 0) {
            syslog(LOG_ERR, "select failed");
            return -1;
        }

		for (i = 0; i <= maxfd; i++) {
			if (FD_ISSET(i, &rset)) {

				// принять и обработать запрос
                if ((sockfd = accept(i, NULL, NULL)) < 0) {
                    syslog(LOG_ERR, "accept failed");
                    continue;
                }

				pthread_create(&tid, NULL, client_thread, (void *)((long)sockfd));
			}
		}
	}

	return 0;
}

/*
 * Инициализировать файл с идентификатором задания. Установить блокировку
 * для записи, чтобы предотвратить запуск других копий демона.
 */
void init_request(void)
{
	int	n;
	char name[FILENMSZ];

	sprintf(name, "%s/%s", SPOOLDIR, JOBFILE);
	jobfd = open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if (write_lock(jobfd, 0, SEEK_SET, 0) < 0) {
        syslog(LOG_ERR, "daemon already running");
        exit(EXIT_FAILURE);
    }

	// повторно использовать буфер с именем файла для счетчика заданий.
    if ((n = read(jobfd, name, FILENMSZ)) < 0) {
        syslog(LOG_ERR, "can't read job file");
        return;
    }

	if (n == 0)
		nextjob = 1;
	else
		nextjob = atol(name);
}

/*
 * Инициализирует информацию о принтере из конфига
 */
void init_printer(void)
{
	printer = get_printaddr();

	if (printer == NULL)
        exit(1);	// сообщение уже выведено в журнал

	printer_name = printer->ai_canonname;

	if (printer_name == NULL)
		printer_name = "printer";

	syslog(LOG_INFO, "printer is %s", printer_name);
}

/*
 * Обновить идентификатор следующего задания в файле.
 * Не обрабатывать случаи переполнения.
 */
void update_jobno(void)
{
	char buf[32];

    if (lseek(jobfd, 0, SEEK_SET) == -1) {
        syslog(LOG_ERR, "can't seek in job file");
        exit(1);
    }

	sprintf(buf, "%d", nextjob);

    if (write(jobfd, buf, strlen(buf)) < 0) {
        syslog(LOG_ERR, "can't update job file");
        exit(1);
    }
}

/*
 * Получить номер следующего задания.
 */
int32_t get_newjobno(void)
{
	int32_t	jobid;

	pthread_mutex_lock(&joblock);
	jobid = nextjob++;

	if (nextjob <= 0)
		nextjob = 1;

	pthread_mutex_unlock(&joblock);

	return jobid;
}

//TODO разобраться со списками
/*
 * Добавляет в очередь новое задание. После этого посылает
 * потоку принтера сигнал о том, что появилось новое задание.
 */
void add_job(struct printreq *reqp, int32_t jobid)
{
	struct job *jp;

    if ((jp = malloc(sizeof(struct job))) == NULL) {
        syslog(LOG_ERR, "malloc failed");
        exit(1);
    }

	memcpy(&jp->req, reqp, sizeof(struct printreq));
	jp->jobid = jobid;
	jp->next = NULL;
	pthread_mutex_lock(&joblock);
	jp->prev = jobtail;

	if (jobtail == NULL)
		jobhead = jp;
	else
		jobtail->next = jp;

	jobtail = jp;

	pthread_mutex_unlock(&joblock);
	pthread_cond_signal(&jobwait);
}

/*
 * Вставить задание в начало списка.
 */
void replace_job(struct job *jp)
{
	pthread_mutex_lock(&joblock);
	jp->prev = NULL;
	jp->next = jobhead;

	if (jobhead == NULL)
		jobtail = jp;
	else
		jobhead->prev = jp;

	jobhead = jp;
	pthread_mutex_unlock(&joblock);
}

/*
 * Удалить задание из очереди
 */
void remove_job(struct job *target)
{
	if (target->next != NULL)
		target->next->prev = target->prev;
	else
		jobtail = target->prev;

	if (target->prev != NULL)
		target->prev->next = target->next;
	else
		jobhead = target->next;
}

/*
 * Проверить при запуске каталог очереди на наличие заданий
 */
void build_qonstart(void)
{
	DIR	*dirp;
    int32_t	jobid;
    int	fd, err, nr;
	struct dirent *entp;
	struct printreq	req;
	char dname[FILENMSZ], fname[FILENMSZ];

	sprintf(dname, "%s/%s", SPOOLDIR, REQDIR);

	if ((dirp = opendir(dname)) == NULL)
		return;

	while ((entp = readdir(dirp)) != NULL) {
		if (strcmp(entp->d_name, ".") == 0 ||
		  strcmp(entp->d_name, "..") == 0)
			continue;

		sprintf(fname, "%s/%s/%s", SPOOLDIR, REQDIR, entp->d_name);

		if ((fd = open(fname, O_RDONLY)) < 0)
			continue;

		nr = read(fd, &req, sizeof(struct printreq));
		if (nr != sizeof(struct printreq)) {
			if (nr < 0)
				err = errno;
			else
				err = EIO;
			close(fd);

			syslog(LOG_ERR, "build_qonstart: can't read %s: %s", fname, strerror(err));
			unlink(fname);

			sprintf(fname, "%s/%s/%s", SPOOLDIR, DATADIR, entp->d_name);
			unlink(fname);

			continue;
		}

		jobid = atol(entp->d_name);
		syslog(LOG_ERR, "adding job %d to queue", jobid);
		add_job(&req, jobid);
	}
	closedir(dirp);
}

/*
 * Принять задание печати от клиента.
 */
void *client_thread(void *arg)
{
	int	n, fd, sockfd, nr, nw, first, err;
	int32_t jobid;
	pthread_t tid;
	struct printreq	req;
	struct printresp res;
	char name[FILENMSZ];
	char buf[IOBUFSZ];

	tid = pthread_self();
	pthread_cleanup_push(client_cleanup, (void *)((long)tid));
	sockfd = (long)arg;
	add_worker(tid, sockfd);

    // прочитать заголовок запроса.
	if ((n = treadn(sockfd, &req, sizeof(struct printreq), 10)) != sizeof(struct printreq)) {
		res.jobid = 0;

		if (n < 0)
			err = errno;
		else
			err = EIO;

		res.retcode = htonl(err);

		strncpy(res.msg, strerror(err), MSGLEN_MAX);
		sendall(sockfd, &res, sizeof(struct printresp));
		pthread_exit((void *)1);
	}

	req.size = ntohl(req.size);
	req.flags = ntohl(req.flags);

	// создать файл данных
	jobid = get_newjobno();
	sprintf(name, "%s/%s/%d", SPOOLDIR, DATADIR, jobid);

	fd = creat(name, FILEPERM);
	if (fd < 0) {
		res.jobid = 0;
        err = errno;
		res.retcode = htonl(err);

		syslog(LOG_ERR, "client_thread: can't create %s: %s", name, strerror(err));
		strncpy(res.msg, strerror(err), MSGLEN_MAX);
		sendall(sockfd, &res, sizeof(struct printresp));
		pthread_exit((void *)1);
	}

	// прочитать файл и сохранить его в каталоге очереди печати,
	// попытаться определить тип файла: PostScript или
	// простой текстовый файл.
	first = 1;
	while ((nr = tread(sockfd, buf, IOBUFSZ, 20)) > 0) {
		if (first) {
			first = 0;
			if (strncmp(buf, "%!PS", 4) != 0)
				req.flags |= PR_TEXT;
		}

		nw = write(fd, buf, nr);
		if (nw != nr) {
			res.jobid = 0;
			if (nw < 0)
				err = errno;
			else
				err = EIO;

            res.retcode = htonl(err);
			syslog(LOG_ERR, "client_thread: can't write %s: %s", name, strerror(err));
            close(fd);

			strncpy(res.msg, strerror(err), MSGLEN_MAX);
			sendall(sockfd, &res, sizeof(struct printresp));
			unlink(name);
			pthread_exit((void *)1);
		}
	}
	close(fd);

	 // создать управляющий файл, затем записать информацию
     // о запросе на печать в управляющий файл
	sprintf(name, "%s/%s/%d", SPOOLDIR, REQDIR, jobid);

	fd = creat(name, FILEPERM);
	if (fd < 0) {
		res.jobid = 0;
        err = errno;
		res.retcode = htonl(err);

		syslog(LOG_ERR, "client_thread: can't create %s: %s", name, strerror(err));
		strncpy(res.msg, strerror(err), MSGLEN_MAX);
		sendall(sockfd, &res, sizeof(struct printresp));

		sprintf(name, "%s/%s/%d", SPOOLDIR, DATADIR, jobid);
		unlink(name);
		pthread_exit((void *)1);
	}

	nw = write(fd, &req, sizeof(struct printreq));
	if (nw != sizeof(struct printreq)) {
		res.jobid = 0;
		if (nw < 0)
			err = errno;
		else
			err = EIO;

        res.retcode = htonl(err);
		syslog(LOG_ERR, "client_thread: can't write %s: %s", name, strerror(err));
		close(fd);
		strncpy(res.msg, strerror(err), MSGLEN_MAX);
		sendall(sockfd, &res, sizeof(struct printresp));
		unlink(name);

		sprintf(name, "%s/%s/%d", SPOOLDIR, DATADIR, jobid);
		unlink(name);
		pthread_exit((void *)1);
	}
	close(fd);

	// отправить ответ клиенту.
	res.retcode = 0;
	res.jobid = htonl(jobid);
	sprintf(res.msg, "request ID %d", jobid);
	sendall(sockfd, &res, sizeof(struct printresp));

	// оповестить поток обслуживания принтера и коррекно выйти.
	syslog(LOG_ERR, "adding job %d to queue", jobid);
	add_job(&req, jobid);

    pthread_cleanup_pop(1);
	return((void *)0);
}

/*
 * Добавить новый поток в список рабочих потоков
 */
void add_worker(pthread_t tid, int sockfd)
{
	struct worker_thread *wtp;

	if ((wtp = malloc(sizeof(struct worker_thread))) == NULL) {
		syslog(LOG_ERR, "add_worker: can't malloc");
		pthread_exit((void *)1);
	}

	wtp->tid = tid;
	wtp->sockfd = sockfd;
	pthread_mutex_lock(&workerlock);
	wtp->prev = NULL;
	wtp->next = workers;

	if (workers == NULL)
		workers = wtp;
	else
		workers->prev = wtp;

	pthread_mutex_unlock(&workerlock);
}

/*
 * Завершить все имеющиеся рабочие потоки
 */
void kill_workers(void)
{
	struct worker_thread *wtp;

	pthread_mutex_lock(&workerlock);

	for (wtp = workers; wtp != NULL; wtp = wtp->next)
		pthread_cancel(wtp->tid);

	pthread_mutex_unlock(&workerlock);
}

/*
 * Процедура выхода для рабочего потока.
 */
void client_cleanup(void *arg)
{
	struct worker_thread *wtp;
	pthread_t tid;

	tid = (pthread_t)((long)arg);
	pthread_mutex_lock(&workerlock);

	for (wtp = workers; wtp != NULL; wtp = wtp->next) {
		if (wtp->tid == tid) {
			if (wtp->next != NULL)
				wtp->next->prev = wtp->prev;

			if (wtp->prev != NULL)
				wtp->prev->next = wtp->next;

			else
				workers = wtp->next;
			break;
		}
	}

	pthread_mutex_unlock(&workerlock);

	if (wtp != NULL) {
		close(wtp->sockfd);
		free(wtp);
	}
}

/*
 * Обслуживание сигналов.
 */
void *signal_thread(void *arg)
{
	int	err, signo;

	for (;;) {
		err = sigwait(&mask, &signo);
        if (err != 0) {
            syslog(LOG_ERR, "sigwait failed: %s", strerror(err));
            exit(1);
        }

		switch (signo) {
		case SIGHUP:
			 // запланировать чтение конфигурационного файла.
			pthread_mutex_lock(&configlock);
			reread = 1;
			pthread_mutex_unlock(&configlock);
			break;

		case SIGTERM:
			kill_workers();
			syslog(LOG_ERR, "terminate with signal %s", strsignal(signo));
			exit(0);

		default:
			kill_workers();
			syslog(LOG_ERR, "unexpected signal %d", signo);
            exit(1);
		}
	}
}

/*
 * Добавить атрибут в заголовок IPP
 */
char *add_option(char *cp, int tag, char *optname, char *optval)
{
	int	n;
	union {
		int16_t s;
		char c[2];
	} u;

	*cp++ = tag;
	n = strlen(optname);
	u.s = htons(n);

	*cp++ = u.c[0];
	*cp++ = u.c[1];

	strcpy(cp, optname);

	cp += n;

	n = strlen(optval);
	u.s = htons(n);

	*cp++ = u.c[0];
	*cp++ = u.c[1];

	strcpy(cp, optval);

	return cp + n;
}

/*
 * Единственный поток, который занимается взаимодействием с принтером.
 */
void *printer_thread(void *arg)
{
	struct job *jp;
	int	hlen, ilen, sockfd, fd, nr, nw, extra;
	char *icp, *hcp, *p;
	struct ipp_hdr *hp;
	struct stat	sbuf;
	struct iovec iov[2];
	char name[FILENMSZ];
	char hbuf[HBUFSZ];
	char ibuf[IBUFSZ];
	char buf[IOBUFSZ];
	char str[64];
    //TODO почему не использовать sleep??
	struct timespec	ts = {60, 0};		// 1 минута

	for (;;) {
		 // получить задание печати.
		pthread_mutex_lock(&joblock);

		while (jobhead == NULL) {
			syslog(LOG_INFO, "printer_thread: waiting...");
			//TODO разобраться с этой функцией
            pthread_cond_wait(&jobwait, &joblock);
		}

		remove_job(jp = jobhead);
		syslog(LOG_INFO, "printer_thread: picked up job %d", jp->jobid);
		pthread_mutex_unlock(&joblock);
		update_jobno();

		 // проверить наличие изменений в конфигурационном файле.
		pthread_mutex_lock(&configlock);

		if (reread) {
			freeaddrinfo(printer);
			printer = NULL;
			printer_name = NULL;
			reread = 0;
			pthread_mutex_unlock(&configlock);
			init_printer();
		} else {
			pthread_mutex_unlock(&configlock);
		}

		// отправить задание принтеру.
		sprintf(name, "%s/%s/%d", SPOOLDIR, DATADIR, jp->jobid);
		if ((fd = open(name, O_RDONLY)) < 0) {
			syslog(LOG_INFO, "job %d canceled - can't open %s: %s", jp->jobid, name, strerror(errno));
			free(jp);
			continue;
		}

		if (fstat(fd, &sbuf) < 0) {
			syslog(LOG_INFO, "job %d canceled - can't fstat %s: %s", jp->jobid, name, strerror(errno));
			free(jp);
			close(fd);
			continue;
		}

		if ((sockfd = connect_retry(AF_INET, SOCK_STREAM, 0, printer->ai_addr, printer->ai_addrlen)) < 0) {
			syslog(LOG_INFO, "job %d deferred - can't contact printer: %s", jp->jobid, strerror(errno));
			goto defer;
		}


		// собраем IPP заголовок.
		icp = ibuf;
		hp = (struct ipp_hdr *)icp;
		hp->major_version = 1;
		hp->minor_version = 1;
		hp->operation = htons(OP_PRINT_JOB);
		hp->request_id = htonl(jp->jobid);
		icp += offsetof(struct ipp_hdr, attr_group);
		*icp++ = TAG_OPERATION_ATTR;
		icp = add_option(icp, TAG_CHARSET, "attributes-charset", "utf-8");
		icp = add_option(icp, TAG_NATULANG, "attributes-natural-language", "en-us");
		sprintf(str, "http://%s/ipp", printer_name);
		icp = add_option(icp, TAG_URI, "printer-uri", str);
		icp = add_option(icp, TAG_NAMEWOLANG, "requesting-user-name", jp->req.usernm);
		icp = add_option(icp, TAG_NAMEWOLANG, "job-name", jp->req.jobnm);
		if (jp->req.flags & PR_TEXT) {
			p = "text/plain";
			extra = 1;
		} else {
			p = "application/postscript";
			extra = 0;
		}
		icp = add_option(icp, TAG_MIMETYPE, "document-format", p);
		*icp++ = TAG_END_OF_ATTR;
		ilen = icp - ibuf;

		// собираем HTTP заголовок.
		//TODO вместо strlen можно использовать результат sprintf
        hcp = hbuf;
		sprintf(hcp, "POST /ipp HTTP/1.1\r\n");
		hcp += strlen(hcp);
		sprintf(hcp, "Content-Length: %ld\r\n", (long)sbuf.st_size + ilen + extra);
		hcp += strlen(hcp);
		strcpy(hcp, "Content-Type: application/ipp\r\n");
		hcp += strlen(hcp);
		sprintf(hcp, "Host: %s:%d\r\n", printer_name, IPP_PORT);
		hcp += strlen(hcp);
		*hcp++ = '\r';
		*hcp++ = '\n';
		hlen = hcp - hbuf;


		 // передать сначала заголовки, потом файл.
		iov[0].iov_base = hbuf;
		iov[0].iov_len = hlen;
		iov[1].iov_base = ibuf;
		iov[1].iov_len = ilen;
		if (writev(sockfd, iov, 2) != hlen + ilen) {
			syslog(LOG_INFO, "can't write to printer");
			goto defer;
		}

		if (jp->req.flags & PR_TEXT) {
			 // хак: позволить печатать PostScript как простой текст
			if (write(sockfd, "\b", 1) != 1) {
				syslog(LOG_INFO, "can't write to printer");
				goto defer;
			}
		}

		while ((nr = read(fd, buf, IOBUFSZ)) > 0) {
			if ((nw = sendall(sockfd, buf, nr)) != nr) {
				if (nw < 0)
				  syslog(LOG_INFO, "can't write to printer");
				else
				  syslog(LOG_INFO, "short write (%d/%d) to printer", nw, nr);
				goto defer;
			}
		}

		if (nr < 0) {
			syslog(LOG_INFO, "can't read %s", name);
			goto defer;
		}

		// прочитать ответ принтера
		if (printer_status(sockfd, jp)) {
			unlink(name);
			sprintf(name, "%s/%s/%d", SPOOLDIR, REQDIR, jp->jobid);
			unlink(name);
			free(jp);
			jp = NULL;
		}
defer:
		close(fd);

		if (sockfd >= 0)
			close(sockfd);

		if (jp != NULL) {
			replace_job(jp);
			nanosleep(&ts, NULL);
		}
	}
}

/*
 * Прочитать данные из принтера – возможно, увеличивая приемный буфер.
 * Возвращает смещение конца данных в буфере или -1 в случае ошибки.
 */
ssize_t readmore(int sockfd, char **bpp, int off, int *bszp)
{
	ssize_t	nr;
    int	bsz = *bszp;
	char *bp = *bpp;

	if (off >= bsz) {
		bsz += IOBUFSZ;

        if ((bp = realloc(*bpp, bsz)) == NULL) {
            syslog(LOG_ERR, "readmore: can't allocate bigger read buffer");
            exit(1);
        }

		*bszp = bsz;
		*bpp = bp;
	}

	if ((nr = tread(sockfd, &bp[off], bsz-off, 1)) > 0)
		return off + nr;
	else
		return -1;
}

/*
 * Прочитать и проанализировать ответ принтера. Вернуть 1, если ответ
 * свидетельствует об успехе, 0 – в противном случае.
 */
int printer_status(int sfd, struct job *jp)
{
    ssize_t	nr;
    int32_t	jobid;
    struct ipp_hdr *hp;
	int	i, success, code, len, found, bufsz, datsz;
	char *bp, *cp, *statcode, *reason, *contentlen;

	/*
     * Прочитать заголовок HTTP и следующий за ним заголовок IPP.
     * Для их получения может понадобиться несколько попыток чтения.
     * Для определения объема читаемых данных используется Content-Length.
	 */
	success = 0;
	bufsz = IOBUFSZ;
    if ((bp = malloc(IOBUFSZ)) == NULL) {
        syslog(LOG_ERR, "printer_status: can't allocate read buffer");
        exit(1);
    }

	while ((nr = tread(sfd, bp, bufsz, 5)) > 0) {
		/*
         * Отыскать код статуса. Ответ начинается со строки "HTTP/x.y",
         * поэтому нужно пропустить 8 символов.
		 */
		cp = bp + 8;
		datsz = nr;
		while (isspace((int)*cp))
			cp++;
		statcode = cp;
		while (isdigit((int)*cp))
			cp++;
		if (cp == statcode) { // неверный формат, записать его в журнал
			syslog(LOG_INFO, "%s", bp);
		} else {
			*cp++ = '\0';
			reason = cp;

			while (*cp != '\r' && *cp != '\n')
				cp++;

			*cp = '\0';
			code = atoi(statcode);

			if (HTTP_INFO(code))
				continue;

			if (!HTTP_SUCCESS(code)) { // возможная ошибка: записать ее
				bp[datsz] = '\0';
				syslog(LOG_INFO, "error: %s", reason);
				break;
			}

			/*
             * Заголовок HTTP в порядке, но нам нужно проверить статус
             * IPP. Для начала найдем атрибут Content-Length.
			 */
			i = cp - bp;
			for (;;) {
				while (*cp != 'C' && *cp != 'c' && i < datsz) {
					cp++;
					i++;
				}

				if (i >= datsz) {	// продолжить чтение заголовка
					if ((nr = readmore(sfd, &bp, i, &bufsz)) < 0) {
						goto out;
					} else {
						cp = &bp[i];
						datsz += nr;
					}
				}

				if (strncasecmp(cp, "Content-Length:", 15) == 0) {
					cp += 15;

					while (isspace((int)*cp))
						cp++;

					contentlen = cp;

					while (isdigit((int)*cp))
						cp++;

					*cp++ = '\0';
					i = cp - bp;
					len = atoi(contentlen);
					break;
				} else {
					cp++;
					i++;
				}
			}
			if (i >= datsz) {	// продолжить чтение заголовка
				if ((nr = readmore(sfd, &bp, i, &bufsz)) < 0) {
					goto out;
				} else {
					cp = &bp[i];
					datsz += nr;
				}
			}

			found = 0;
			while (!found) {  // поиск конца заголовка HTTP
				while (i < datsz - 2) {
					if (*cp == '\n' && *(cp + 1) == '\r' && *(cp + 2) == '\n') {
						found = 1;
						cp += 3;
						i += 3;
						break;
					}
					cp++;
					i++;
				}
				if (i >= datsz) {	// продолжить чтение заголовка
					if ((nr = readmore(sfd, &bp, i, &bufsz)) < 0) {
						goto out;
					} else {
						cp = &bp[i];
						datsz += nr;
					}
				}
			}

			if (datsz - i < len) {	// продолжить чтение заголовка
				if ((nr = readmore(sfd, &bp, i, &bufsz)) < 0) {
					goto out;
				} else {
					cp = &bp[i];
					datsz += nr;
				}
			}

			hp = (struct ipp_hdr *)cp;
			i = ntohs(hp->status);
			jobid = ntohl(hp->request_id);

			if (jobid != jp->jobid) {
				// другие задания, игнорировать их
				syslog(LOG_INFO, "jobid %d status code %d", jobid, i);
				break;
			}

			if (STATCLASS_OK(i))
				success = 1;
			break;
		}
	}

out:
	free(bp);
	if (nr < 0)
		syslog(LOG_INFO, "jobid %d: error reading printer response: %s", jobid, strerror(errno));

	return success;
}
