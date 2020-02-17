#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#define BUF_SIZE 4096

int main(void)
{
	struct sockaddr_nl nls;
	struct pollfd pfd;
	char buf[BUF_SIZE];
    int res, len, i;

	pfd.events = POLLIN;
	pfd.fd = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);

	if (pfd.fd == -1) {
        printf("socket error\n");
        return -1;
	}

    memset(&nls, 0, sizeof(struct sockaddr_nl));
    nls.nl_family = AF_NETLINK;
    nls.nl_pid = getpid();
    nls.nl_groups = -1;

    if (bind(pfd.fd, (void *)&nls, sizeof(struct sockaddr_nl))) {
        printf("bind failed\n");
        return -1;
    }

    while (1) {
        res = poll(&pfd, 1, 10000); // wait 10 sec

        if (res < 0) {
            printf("poll error\n");
            return -1;
        }

        if (res == 0) {
            printf("time is up\n");
            return 0;
        }

        len = read(pfd.fd, buf, sizeof(buf));

        printf("======== Received %d bytes========\n", len);
        for (i = 0; i < len; ++i) {
            if (buf[i] == 0)
                printf("[0x00]\n");
            else if (buf[i] < 33 || buf[i] > 126)
                printf("[0x%02hhx]", buf[i]);
            else
                printf("%c", buf[i]);
        }
        printf("==================================\n\n");
    }
	return 0;
}
