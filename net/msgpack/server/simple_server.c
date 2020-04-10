#include <msgpack/object.h>
#include <msgpack/unpack.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <msgpack.h>
#include <fcntl.h>

#include "../functions.h"

static void write_data(struct msgpack_object_kv *kv)
{
    msgpack_object key = kv->key;
    msgpack_object val = kv->val;

    const char *fname = key.via.str.ptr;

    int fd = open(fname, O_WRONLY | O_APPEND | O_CREAT, 0644);

    if (fd < 0) {
        perror("open()");
        return;
    }

    const char *fdata = val.via.bin.ptr;
    uint32_t fsize = val.via.bin.size;

   if (write(fd, fdata, fsize) < 0)
       perror("write()");

   close(fd);
}

int main(void)
{
    char buf[KBYTE];
    struct sockaddr_storage cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    char host[INET_ADDRSTRLEN], port[PORTLEN];

	int server_sock = open_listen_fd(SERV_PORT);

	if (server_sock < 0)
		exit(1);

	for (;;) {
        char *dynbuf = NULL;
        ssize_t nbytes, dynsize = 0;

        int client_sock = accept(server_sock, (struct sockaddr *)&cliaddr, &clilen);

		if (client_sock < 0) {
			perror("accept error");
			exit(1);
		}

        getnameinfo((struct sockaddr *)&cliaddr, clilen, host, sizeof(host),
        port, sizeof(port), NI_NUMERICHOST | NI_NUMERICSERV);

        printf("new connection from %s:%s\n", host, port);

        while ((nbytes = recv(client_sock, buf, sizeof(buf), 0)) > 0 ) {
            dynsize += nbytes;
            dynbuf = realloc(dynbuf, dynsize);

            ssize_t offset = dynsize - nbytes;
            memcpy(dynbuf+offset, buf, nbytes);
        }

        close(client_sock);

        msgpack_unpacked msg;
        msgpack_unpacked_init(&msg);

        if (msgpack_unpack_next(&msg, dynbuf, dynsize, NULL)) {
            msgpack_object data = msg.data;

            if (data.type == MSGPACK_OBJECT_MAP)
                write_data(data.via.map.ptr);
            else
                printf("data type is not 'map' = %d\n", data.type);
        }

        free(dynbuf);
        msgpack_unpacked_destroy(&msg);
	}

	return 0;
}
