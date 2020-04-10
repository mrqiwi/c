#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <msgpack.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "../functions.h"

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: progname filename\n");
        return -1;
    }

    char buf[KBYTE];
    char *fname = argv[1];
    char *dynbuf = NULL;
    ssize_t nbytes, dynsize = 0;

    int fd = open(fname, O_RDONLY);

    if (fd < 0) {
        perror("open() error");
        exit(1);
    }

    while ((nbytes = read(fd, buf, sizeof(buf))) > 0 ) {
        dynsize += nbytes;
        dynbuf = realloc(dynbuf, dynsize);

        ssize_t offset = dynsize - nbytes;
        memcpy(dynbuf+offset, buf, nbytes);
    }

    close(fd);

    msgpack_sbuffer *buffer = msgpack_sbuffer_new();
    msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

    msgpack_pack_map(pk, 1);
    msgpack_pack_str(pk, strlen(fname)+1);
    msgpack_pack_str_body(pk, fname, strlen(fname)+1);
    msgpack_pack_bin(pk, dynsize);
    msgpack_pack_bin_body(pk, dynbuf, dynsize);

    int sock = open_connection(SERV_ADDR, SERV_PORT);

    if (sock < 0)
        exit(1);

    send(sock, buffer->data, buffer->size, 0);

    msgpack_sbuffer_clear(buffer);
    msgpack_packer_free(pk);
    close(sock);

    return 0;
}

