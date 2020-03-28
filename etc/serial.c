#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>

#define AT_OK               "\r\nOK\r\n"
#define AT_ERROR            "\r\nERROR\r\n"
#define AT_LONG_ERROR       "\r\n+CME ERROR: "
#define AT_LONG_CMS_ERROR   "\r\n+CMS ERROR: "
#define AT_PROMPT           "\r\n>"


int at_do(int fd, char *cmd, int attempts)
{
	char buf[4096] = {0};
	int tail, written, n, readed = 0;
	struct timeval tv;
	fd_set rset;

	tail = (int)strlen(cmd);
	written = 0;

	while (tail > 0) {

		n = write(fd, cmd + written, tail);

		if (n < 0) {
			usleep(1000);
			continue;
		}

		written += n;
		tail -= n;
	}

	fsync(fd);

	FD_ZERO(&rset);

	while (attempts > 0) {

		FD_SET(fd, &rset);
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		n = select(fd + 1, &rset, NULL, NULL, &tv);

		if (n < 0) {
	    	printf("Select error: %s", strerror(errno));
	    	break;
		} else if (n == 0) {
	    	printf("Timeout: %d\n", attempts);
			attempts--;
			continue;
		}

		n = read(fd, buf + readed, sizeof(buf)-(readed+1)); // +1 для '\0'

		if (n < 0) {
	    	printf("Read error: %s", strerror(errno));
	    	break;
		} else if (n == 0) {
	    	printf("No data");
			attempts--;
			continue;
		}

		readed += n;

		if (strstr(buf, AT_OK))
			break;
		else if (strstr(buf, AT_ERROR) || strstr(buf, AT_LONG_ERROR) || strstr(buf, AT_LONG_CMS_ERROR))
			break;
		else if(strstr(buf, AT_PROMPT))
			break;
	}

	printf("%s", buf);

	return 0;
}


int tty_prepare(int fd, struct termios *tty)
{
	memset(tty, 0, sizeof(*tty));

	if(tcgetattr(fd, tty) != 0) {
	    printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
	    return -1;
	}

	tty->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	tty->c_oflag &= ~OPOST;
	tty->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	tty->c_cflag &= ~(CSIZE|PARENB|CSTOPB);
	tty->c_cflag |=  (CS8|CLOCAL|CREAD);

	// tty->c_cc[VTIME] = 10;    // 1 секунда
	// tty->c_cc[VMIN] = 0;

	cfsetispeed(tty, B9600);
	cfsetospeed(tty, B9600);

	return tcsetattr(fd, TCSANOW, tty);
}


int connect(int number, char *cmd)
{
	char path[16] = {0}, at_cmd[4096] = {0};
	int serial_port;
	struct termios tty;

	snprintf(path, sizeof(path), "/dev/ttyUSB%d", number);
	snprintf(at_cmd, sizeof(at_cmd), "%s\r", cmd);

	serial_port = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK/* | O_EXCL*/);

	if (tty_prepare(serial_port, &tty) < 0)
		return -1;

	at_do(serial_port, at_cmd, 5);

	close(serial_port);

	return 0;
}


int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("usage: um 1 cmd\n");
		printf("1  - number ttyUSB\n");
		printf("at - at command\n");
		return -1;
	}

	int number = atoi(argv[1]);
	char *cmd = argv[2];

	connect(number, cmd);

	return 0;
}