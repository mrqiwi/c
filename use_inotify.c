#include <stdio.h>
#include <sys/inotify.h>
#include <unistd.h>

#define EVENT_SIZE  (sizeof(struct inotify_event))
#define BUF_LEN     (1024 * (EVENT_SIZE + 16))

int main(void) {
    int nbytes, fd, wd;
    char buffer[BUF_LEN];
	const char *path = "/data/c";

	fd = inotify_init();

	if (fd < 0) {
		perror("inotify_init");
		return -1;
	}

	wd = inotify_add_watch(fd, path, IN_MODIFY | IN_CREATE | IN_DELETE);

	if (wd < 0) {
    	perror("inotify_add_watch");
    	return -1;
    }

	while (1) {
		nbytes = read(fd, buffer, BUF_LEN);

		if (nbytes < 0) {
			perror("read");
			return -1;
		}

		struct inotify_event *event = (struct inotify_event *) buffer;

		if (event->len) {
            if (event->mask & IN_CREATE) {
                printf("The file %s was created.\n", event->name);
            } else if (event->mask & IN_DELETE) {
                printf("The file %s was deleted.\n", event->name);
            } else if (event->mask & IN_MODIFY) {
                printf("The file %s was modified.\n", event->name);
            }
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);

    return 0;
}
