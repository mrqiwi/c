#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mopen.h"

int main(int argc, char *argv[])
{
    int fd, res, len;
    char msg[160];
    char dev[80] = "/dev/";

    strcat(dev, DEVNAM);

    if ((fd = open(dev, O_RDWR)) < 0) {
        printf("open device error: %m");
        return -1;
    }

    printf( "> ");
    fflush(stdout);

    fgets(msg, sizeof(msg), stdin);

    len = strlen(msg);
    if ((res = write(fd, msg, len)) != len)
        printf("write device error: %m");

    char *p = msg;
    do {
        if ((res = read(fd, p, sizeof(msg))) > 0) {
            *(p + res) = '\0';
            printf("read %d bytes: %s", res, p);
            p += res;
        } else if (res < 0)
            printf("read device error: %m");

    } while (res > 0);

    printf("%s", msg);
    close(fd);

    return 0;
};
