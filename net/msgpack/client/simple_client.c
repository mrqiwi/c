#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <msgpack.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#include "../functions.h"

int main(void)
{
    int nbytes;
    char buf[MAXLINE];
    char fname[] = "night.jpg";

    int sock = open_connection(SERV_ADDR, SERV_PORT);

    if (sock < 0)
        exit(1);

    int fd = open(fname, O_RDONLY);

    if (fd < 0) {
        perror("open() error");
        exit(1);
    }


    while ((nbytes = read(fd, buf, sizeof(buf))) > 0 ) {
        printf("---nbytes = %d--\n", nbytes);

        msgpack_sbuffer *buffer = msgpack_sbuffer_new();
        msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

        msgpack_pack_map(pk, 1);
        msgpack_pack_str(pk, sizeof(fname));
        msgpack_pack_str_body(pk, fname, sizeof(fname));
        msgpack_pack_bin(pk, nbytes);
        msgpack_pack_bin_body(pk, buf, nbytes);

        send(sock, buffer->data, buffer->size, 0);

        printf("---buffer->size = %lu--\n", buffer->size);

        /* memset(buf, 0, sizeof(buf)); */
        msgpack_sbuffer_clear(buffer);
        msgpack_packer_free(pk);
    }


    /* if (send(sock, buffer->data, buffer->size, 0) < 0) { */
    /*     perror("cannot send data"); */
    /*     exit(1); */
    /* } */

    /* msgpack_sbuffer_clear(buffer); */
    /* msgpack_packer_free(pk); */
    close(fd);
    close(sock);

    return 0;
}

