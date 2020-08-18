/*
 * Утилита печати документов. Открывает файл и отправляет демону печати.
 * 	print [-t] filename
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "print.h"

/*
 * Необходимо для функций журналирования.
 */
int log_to_stderr = 1;

static void submit_file(int, int, const char *, size_t, int);

int main(int argc, char *argv[])
{
    char *host;
    struct stat	sbuf;
    int	fd, sfd, err, is_text, c;
    struct addrinfo	*ailist, *aip;

    err = 0;
    is_text = 0;

    while ((c = getopt(argc, argv, "t")) != -1) {
        switch (c) {
            case 't':
                is_text = 1;
                break;

            case '?':
                err = 1;
                break;
        }
    }

    if (err || (optind != argc - 1)) {
        printf("usage: print [-t] filename\n");
        return -1;
    }

    if ((fd = open(argv[optind], O_RDONLY)) < 0) {
        printf("can't open %s\n", argv[optind]);
        return -1;
    }

    if (fstat(fd, &sbuf) < 0) {
        printf("can't stat %s\n", argv[optind]);
        return -1;
    }

    if (!S_ISREG(sbuf.st_mode)) {
        printf("%s must be a regular file\n", argv[optind]);
        return -1;
    }

    if ((host = get_printserver()) == NULL) {
        printf("no print server defined\n");
        return -1;
    }

    if ((err = getaddrlist(host, "print", &ailist)) != 0) {
        printf("getaddrinfo error: %s\n", gai_strerror(err));
        return -1;
    }

    for (aip = ailist; aip != NULL; aip = aip->ai_next) {
        if ((sfd = connect_retry(AF_INET, SOCK_STREAM, 0, aip->ai_addr, aip->ai_addrlen)) < 0) {
            err = errno;
        } else {
            submit_file(fd, sfd, argv[optind], sbuf.st_size, is_text);
            return 0;
        }
    }

    printf("can't contact %s", host);
    return -1;
}

/*
 * Отправить файл демону печати.
 */
void submit_file(int fd, int sockfd, const char *fname, size_t fsize, int is_text)
{
    int	nr, nw, len;
    char buf[IOBUFSZ];
    struct passwd *pwd;
    struct printreq	req;
    struct printresp res;
    /*
     * Сначала соберем заголовок.
     */
    if ((pwd = getpwuid(geteuid())) == NULL) {
        strcpy(req.usernm, "unknown");
    } else {
        //TODO правильно ли такое копирование??
        strncpy(req.usernm, pwd->pw_name, USERNM_MAX-1);
        req.usernm[USERNM_MAX-1] = '\0';
    }
    req.size = htonl(fsize);

    if (is_text)
        req.flags = htonl(PR_TEXT);
    else
        req.flags = 0;

    if ((len = strlen(fname)) >= JOBNM_MAX) {
        /*
         * Усечь имя файла (с учетом 5 символов, отводимых под
         * четыре символа префикса и завершающий нулевой символ).
         */
        strcpy(req.jobnm, "... ");
        strncat(req.jobnm, &fname[len-JOBNM_MAX+5], JOBNM_MAX-5);
    } else {
        strcpy(req.jobnm, fname);
    }

    /*
     * Отправляем заголовок на сервер.
     */
    nw = sendall(sockfd, &req, sizeof(struct printreq));

    if (nw != sizeof(struct printreq)) {
        if (nw < 0)
            perror("can't write to print server");
        else
            printf("short write (%d/%ld) to print server", nw, sizeof(struct printreq));

        exit(EXIT_FAILURE);
    }

    /*
     * Отправляем файл.
     */
    while ((nr = read(fd, buf, IOBUFSZ)) != 0) {
        nw = sendall(sockfd, buf, nr);
        if (nw != nr) {
            if (nw < 0)
                perror("can't write to print server");
            else
                printf("short write (%d/%d) to print server", nw, nr);

            exit(EXIT_FAILURE);
        }
    }

    /*
     * Прочитать ответ.
     */
    if ((nr = recvall(sockfd, &res, sizeof(struct printresp))) != sizeof(struct printresp)) {
        perror("can't read response from server");
        exit(EXIT_FAILURE);
    }

    if (res.retcode != 0) {
        printf("rejected: %s\n", res.msg);
        exit(EXIT_FAILURE);
    } else {
        printf("job ID %ld\n", (long)ntohl(res.jobid));
    }
}
