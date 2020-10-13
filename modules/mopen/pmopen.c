#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mopen.h"


int prepare(char *test)
{
    int fd, len, res;
    char dev[80] = "/dev/";

    strcat(dev, DEVNAM);

    if ((fd = open(dev, O_RDWR)) < 0)
        printf("open device error: %m\n");

    len = strlen(test);

    if ((res = write(fd, test, len)) != len)
        printf("write device error: %m\n");
    else
        printf("prepared %d bytes: %s\n", res, test);

    return fd;
}

void test(int fd)
{
    char buf[LEN_MSG + 1];
    int res;
    printf("------------------------------------\n");
    do {
        if ((res = read(fd, buf, LEN_MSG)) > 0) {
            buf[res] = '\0';
            printf("read %d bytes: %s\n", res, buf);
        } else if (res < 0)
            printf("read device error: %m\n");
        else
            printf("read end of stream\n");
    } while (res > 0);
    printf("------------------------------------\n");
}

int main(int argc, char *argv[])
{
    int fd1, fd2;  // разные дескрипторы одного устройства

    fd1 = prepare("11111");
    fd2 = prepare("22222");

    test(fd1);
    test(fd2);

    close(fd1);
    close(fd2);

    return 0;
};
