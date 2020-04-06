#include <msgpack/object.h>
#include <msgpack/unpack.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <msgpack.h>
#include <fcntl.h>

#include "../functions.h"

int main(void)
{
	char buffer[4096] = {0};
    char hoststr[INET_ADDRSTRLEN], portstr[INET_ADDRSTRLEN];
	int server_sock, client_sock;
	socklen_t clilen;
    struct sockaddr_storage cliaddr;
    msgpack_object data;
    /* msgpack_zone mempool; */

	server_sock = open_listen_fd(SERV_PORT);

	if (server_sock < 0)
		exit(1);

	for (;;) {
	   	clilen = sizeof cliaddr;
        client_sock = accept(server_sock, (struct sockaddr *)&cliaddr, &clilen);

		if (client_sock < 0) {
			perror("accept error");
			exit(1);
		}

        getnameinfo((struct sockaddr *)&cliaddr, clilen, hoststr, sizeof(hoststr),
                portstr, sizeof(portstr),NI_NUMERICHOST | NI_NUMERICSERV);

        printf("new connection from %s:%s\n", hoststr, portstr);

        int fd = open("night.jpg", O_WRONLY | O_APPEND | O_CREAT, 0644);

        if (fd < 0) {
            perror("open() error");
            exit(1);
        }

        int total = 0;
        int nbytes = 0;

        msgpack_unpacked msg;
        msgpack_unpacked_init(&msg);

        while ((nbytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0 ) {
            total += nbytes;

            if (msgpack_unpack_next(&msg, buffer, sizeof(buffer), NULL)) {
                msgpack_object data = msg.data;

                if (data.type == MSGPACK_OBJECT_MAP) {
                    struct msgpack_object_kv *kv = data.via.map.ptr;

                    msgpack_object key = kv->key;
                    msgpack_object val = kv->val;

                    printf("key type = %d\n", key.type);
                    printf("val type = %d\n", val.type);

                    const char *fname = key.via.str.ptr;
                    printf("from client: %s\n", fname);

                    const char *fdata = val.via.bin.ptr;
                    uint32_t fsize = val.via.bin.size;

                    write(fd, fdata, fsize);
                }
            }
            memset(buffer, 0, sizeof(buffer));
        }
        msgpack_unpacked_destroy(&msg);

        printf("Received bytes: %d\n", total);

        if (nbytes < 0)
            perror("recv() error");

        close(fd);

        /* if (recv(client_sock, buffer, sizeof(buffer), 0) > 0) { */
        /*     msgpack_unpacked msg; */
        /*     msgpack_unpacked_init(&msg); */
        /*  */
        /*     if (msgpack_unpack_next(&msg, buffer, sizeof buffer, NULL)) { */
        /*         msgpack_object data = msg.data; */
        /*  */
        /*         if (data.type == MSGPACK_OBJECT_MAP) { */
        /*             struct msgpack_object_kv *kv = data.via.map.ptr; */
        /*  */
        /*             msgpack_object key = kv->key; */
        /*             msgpack_object val = kv->val; */
        /*  */
        /*             const char *pkey = key.via.str.ptr; */
        /*             const char *pval = val.via.str.ptr; */
        /*  */
        /*             printf("from client: %s\n", pkey); */
        /*             printf("from client: %s\n", pval); */
        /*         } */
        /*     } */
        /*     msgpack_unpacked_destroy(&msg); */
        /* } */

	   close(client_sock);
	}

	return 0;
}


