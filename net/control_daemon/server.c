// gcc volumecontrol.c -o volumecontrol -lasound
// chmod a+s
// /var/log/syslog

#include <stdlib.h>         // for exit
#include <fcntl.h>          // for dup2
#include <sys/stat.h>       // for umask
#include <sys/types.h>      // for size_t, pid_t
#include <sys/wait.h>       // for wait
#include <sys/socket.h>     // for socket
#include <arpa/inet.h>      // for htons, htonl
#include <string.h>         // for memset
#include <errno.h>          // for errno
#include <unistd.h>         // for read
#include <signal.h>	        // for signal
#include <string.h>         // for strcmp
#include <alsa/asoundlib.h> // for sound
#include <alsa/mixer.h>     // for sound
#include <sys/reboot.h>     // for reboot
#include <syslog.h>         // for syslog

#define SERV_PORT 5000
#define LISTENQ   1024
#define	MAXLINE	  1024
#define	MSGSIZE	  2

#define BD_NO_CHDIR           01    // Don't chdir("/") */
#define BD_NO_CLOSE_FILES     02    // Don't close all open files */
#define BD_NO_REOPEN_STD_FDS  04    // Don't reopen stdin, stdout, and stderr to /dev/null */
#define BD_NO_UMASK0         010    // Don't do a umask(0) */
#define BD_MAX_CLOSE        8192    // Maximum file descriptors to close if sysconf(_SC_OPEN_MAX) is indeterminate */

void sig_chld(int signo);
int volume(char *action);
int shut_down(char *action);
int become_daemon(int flags);

typedef struct {
  char *action;
  int (*func)(char*);
} action_t;

action_t act_list[] = {{"+", &volume},
                       {"-", &volume},
                       {"r", &shut_down},
                       {"s", &shut_down},
                       {NULL, NULL}};

int main(void)
{
	char recbuff[MSGSIZE], buff[MAXLINE];
	int serverfd, clientfd, enable = 1;
	socklen_t clilen;
	struct sockaddr_in cliaddr, servaddr;
	pid_t pid;
	size_t nleft;
	ssize_t nreaded;
	char *pbuff;
	struct sigaction act;
	action_t *func_obj;

	if (become_daemon(0) == -1) {
		syslog(LOG_ERR, "Daemon error: %s\n", strerror(errno));
		exit(1);
	}

	serverfd = socket(AF_INET, SOCK_STREAM, 0);

	if (serverfd < 0) {
		syslog(LOG_ERR, "Socket error: %s\n", strerror(errno));
		exit(1);
	}

	if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
		syslog(LOG_ERR, "Setsockopt(SO_REUSEADDR) error: %s\n", strerror(errno));
		exit(1);
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SERV_PORT);

	if (bind(serverfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		syslog(LOG_ERR, "Bind error: %s\n", strerror(errno));
		exit(1);
	}

	if (listen(serverfd, LISTENQ) < 0) {
		syslog(LOG_ERR, "Listen error: %s\n", strerror(errno));
		exit(1);
	}

	// для ожидания дочерних процессов
	act.sa_handler = sig_chld;
	act.sa_flags |= SA_RESTART;
	sigemptyset(&act.sa_mask);

	if (sigaction(SIGCHLD, &act, 0) < 0) {
		syslog(LOG_ERR, "Sigaction error: %s\n", strerror(errno));
		exit(1);
	}

	for (;;) {
	   	clilen = sizeof(cliaddr);
		clientfd = accept(serverfd, (struct sockaddr *)&cliaddr, &clilen);

		if (clientfd < 0) {
			if (errno == EINTR)
				continue;
			else {
				syslog(LOG_ERR, "Accept error: %s\n", strerror(errno));
				exit(1);
			}
		}

		pid = fork();

		if (pid  == -1) {
			syslog(LOG_ERR, "Fork error: %s\n", strerror(errno));
			exit(1);
		}

		if (pid == 0) {
			close(serverfd);

			nleft = sizeof(recbuff)-1;
			pbuff = recbuff;

			while (nleft > 0) {
				nreaded = read(clientfd, pbuff, MSGSIZE);

				if (nreaded < 0) {
					syslog(LOG_ERR, "Read error: %s\n", strerror(errno));
					exit(1);
				} else if ((nreaded == 0) || strstr(pbuff, "\0"))
					break;

				pbuff += nreaded;
				nleft -= nreaded;
			}

		   	syslog(LOG_INFO, "Message '%s' from %s:%d\n", recbuff, inet_ntop(AF_INET, &cliaddr.sin_addr, buff, sizeof(buff)), ntohs(cliaddr.sin_port));

		    for (func_obj = act_list; func_obj->func; func_obj++)
		        if (!strcmp(func_obj->action, recbuff)) {
		            func_obj->func(recbuff);
		            exit(0);
		        }

			syslog(LOG_ERR, "Unknown action: %s\n", recbuff);
			exit(1);
		}
		close(clientfd);
	}

	return 0;
}

void sig_chld(int signo)
{
	(void) signo;
	pid_t pid;
	int stat;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0)
		; // WNOHANG - немедленное возвращение управления, если ни один дочерний процесс не завершил выполнение

  	return;
}

int volume(char *action)
{
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    int res = 0;
    long min, max, cur_val, one_percent, volume;
    const char *card = "default", *selem_name = "Master";

    res = snd_mixer_open(&handle, 0);

    if (res < 0) {
        syslog(LOG_ERR, "error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_attach(handle, card);

    if (res < 0) {
        syslog(LOG_ERR, "error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_selem_register(handle, NULL, NULL);

    if (res < 0) {
        syslog(LOG_ERR, "error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_load(handle);

    if (res < 0) {
        syslog(LOG_ERR, "error: %s\n", snd_strerror(res));
        goto end;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    elem = snd_mixer_find_selem(handle, sid);

    if (!elem) {
        syslog(LOG_ERR, "error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &cur_val);

    if (res < 0) {
        syslog(LOG_ERR, "error: %s\n", snd_strerror(res));
        goto end;
    }

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

    one_percent = max / 100;
    volume = !strcmp(action, "+") ? cur_val + (5 * one_percent) : cur_val - (5 * one_percent);

    if (volume > min && volume < max)
        snd_mixer_selem_set_playback_volume_all(elem, volume);

end:
    snd_mixer_close(handle);
    return res;
}
// TODO разобраться с reboot
int shut_down(char *action)
{
    sync();
    setuid(0);

    if (strcmp(action, "r"))
    	reboot(RB_AUTOBOOT);
    else if (strcmp(action, "s"))
    	reboot(RB_POWER_OFF);

    exit(0);
}

int become_daemon(int flags)
{
    int maxfd, fd;

    switch (fork()) {
	    case -1: return -1;
	    case 0:  break;
	    default: _exit(EXIT_SUCCESS);
    }

    if (setsid() == -1)
        return -1;

    switch (fork()) {
	    case -1: return -1;
	    case 0:  break;
	    default: _exit(EXIT_SUCCESS);
    }

    if (!(flags & BD_NO_UMASK0))
        umask(0);

    if (!(flags & BD_NO_CHDIR))
        chdir("/");

    if (!(flags & BD_NO_CLOSE_FILES)) {
        maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd == -1)
            maxfd = BD_MAX_CLOSE;

        for (fd = 0; fd < maxfd; fd++)
            close(fd);
    }

    if (!(flags & BD_NO_REOPEN_STD_FDS)) {
        close(STDIN_FILENO);

        fd = open("/dev/null", O_RDWR);

        if (fd != STDIN_FILENO)
            return -1;
        if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO)
            return -1;
        if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO)
            return -1;
    }

    return 0;
}
