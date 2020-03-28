// gcc prog -lz
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <zlib.h>
#include <fcntl.h>
#include <unistd.h>

int stream_decompress(gzFile input, int output)
{
    char buf[512];
    int nbytes = -1;

    while (!gzeof(input)) {
        nbytes = gzread(input, buf, sizeof(buf));

        if (nbytes < 0)
            return -1;

        write(output, buf, nbytes);
    }

    return 0;
}

int stream_compress(int input, gzFile output)
{
    char buf[512];
    int nbytes = -1;

    while ((nbytes = read(input, buf, sizeof(buf)))) {
        if (nbytes == -1)
            return -1;

        gzwrite(output, buf, nbytes);
    }

    return 0;
}

int file_convert(const char *src, const char *dst, bool compress)
{
    int open_mode = compress ? (O_RDONLY) : (O_WRONLY | O_CREAT | O_TRUNC);
    const char *gzopen_mode = compress ? "wb" : "rb";
    int fd = -1;
    gzFile gzfd = NULL;
    int ret = -1;

    fd = open(compress ? src : dst, open_mode, 0644);
    gzfd = gzopen(compress ? dst : src, gzopen_mode);

    if (!gzfd || fd < 0)
        goto out;

    ret = compress ? stream_compress(fd, gzfd) : stream_decompress(gzfd, fd);

out:
    if (fd >= 0)
        close(fd);

    if (gzfd)
        gzclose(gzfd);

    if (ret)
        unlink(dst);

    return ret;
}

char *change_name(const char *name, const char *suffix)
{
    char *buf = calloc(PATH_MAX, sizeof(char));

    if (!buf) {
        printf("malloc() error\n");
        return NULL;
    }

    if (suffix) {
        snprintf(buf, PATH_MAX, "%s%s", name, suffix);
    } else {
        strncpy(buf, name, PATH_MAX-1);
        char *ptr = strrchr(buf, '.');
        *ptr = '\0';
    }

    return buf;
}

int main(int argc, char const *argv[])
{
    if (argc != 2) {
        printf("Usage: ./prog filename\n");
        return 1;
    }

    char *new_name = NULL;
    int ret = 0;

    if (strstr(argv[1], ".gz")) {
       new_name = change_name(argv[1], NULL);

        if (!new_name)
            return -1;

        if (file_convert(argv[1], new_name, false) < 0) {
            ret = -1;
            goto out;
        }

    } else {
        new_name = change_name(argv[1], ".gz");

        if (!new_name)
            return -1;

        if (file_convert(argv[1], new_name, true) < 0) {
            ret = -1;
            goto out;
        }
    }

out:
    if (new_name)
        free(new_name);

    return ret;
}