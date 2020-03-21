/* gcc http_server.c -o http_server -levent */
/* http_server 127.0.0.1 8090 */
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <evhttp.h>


static void http_img_cb(struct evhttp_request *request, void *ctx)
{
    int fd;
    int total_read_bytes, read_bytes;
	const char *fname;
    struct stat stbuf;
    struct evbuffer *evb;
	struct evkeyvalq uri_params;

	evb = evbuffer_new();

	printf ("Request from: %s:%d URI: %s\n", request->remote_host, request->remote_port, request->uri);

	evhttp_parse_query(request->uri, &uri_params);
	fname = evhttp_find_header(&uri_params, "name");

	if (!fname) {
	    evbuffer_add_printf(evb, "Bad request");
	    evhttp_send_reply(request, HTTP_BADREQUEST, "Bad request", evb);
	    evhttp_clear_headers(&uri_params);
	    evbuffer_free(evb);
	    return;
	}

	if ((fd = open(fname, O_RDONLY)) < 0) {
	    evbuffer_add_printf(evb, "File %s not found", fname);
	    evhttp_send_reply(request, HTTP_NOTFOUND, "File not found", evb);
	    evhttp_clear_headers(&uri_params);
	    evbuffer_free(evb);
	    return;
	}

	if (fstat(fd, &stbuf) < 0) {
	    evbuffer_add_printf(evb, "File %s not found", fname);
	    evhttp_send_reply(request, HTTP_NOTFOUND, "File not found", evb);
	    evhttp_clear_headers(&uri_params);
	    evbuffer_free(evb);
	    close(fd);
	    return;
	}

	total_read_bytes = 0;

	while (total_read_bytes < stbuf.st_size) {
	    read_bytes = evbuffer_read(evb, fd, stbuf.st_size);

	    if (read_bytes < 0) {
	        evbuffer_add_printf(evb, "Error reading file %s", fname);
	        evhttp_send_reply(request, HTTP_NOTFOUND, "File not found", evb);
	        evhttp_clear_headers(&uri_params);
	        evbuffer_free(evb);
	        close(fd);
	        return;
	    }

	    total_read_bytes += read_bytes;
	}

	evhttp_add_header(request->output_headers, "Content-Type", "image/jpeg");
	evhttp_send_reply(request, HTTP_OK, "HTTP_OK", evb);

	evhttp_clear_headers(&uri_params);
	evbuffer_free(evb);
	close(fd);
}


int main (int argc, char *argv[])
{
	struct event_base *ev_base;
	struct evhttp *ev_http;

	if (argc != 3) {
		printf ("Usage: %s host port\n", argv[0]);
		exit(1);
	}

	ev_base = event_init();
	ev_http = evhttp_new(ev_base);

	if (evhttp_bind_socket (ev_http, argv[1], (u_short)atoi (argv[2]))) {
		printf("Failed to bind %s:%s\n", argv[1], argv[2]);
		exit(1);
	}

	evhttp_set_cb(ev_http, "/img", http_img_cb, NULL);

	event_base_dispatch (ev_base);

	return 0;
}

