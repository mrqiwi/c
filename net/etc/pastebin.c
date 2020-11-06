#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <curl/curl.h>

#define PATTERN "api_option=paste" \
                "&api_user_key=" \
                "&api_paste_private=0"\
                "&api_paste_name=" \
                "&api_paste_expire_date=10M" \
                "&api_paste_format=%s" \
                "&api_dev_key=%s" \
                "&api_paste_code=%s"

static char *generate_request(const char *data, const char *filename)
{
    char *filetype = strrchr(filename, '.');
    if (filetype)
        filetype++;
    else
        filetype = "";

    int length = snprintf(NULL, 0, PATTERN, filetype, getenv("PASTEBIN"), data) + 1;

    char *buffer = malloc(length);
    if (!buffer) {
        perror("malloc() failed");
        return NULL;
    }

    snprintf(buffer, length, PATTERN, filetype, getenv("PASTEBIN"), data);

    return buffer;
}

static char *url_encoded_string(CURL *curl, const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open() failed");
        return NULL;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size < 0) {
        close(fd);
        perror("lseek() failed");
        return NULL;
    }

    char *buffer = (char *)mmap(0, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buffer == MAP_FAILED) {
        close(fd);
        perror("mmap() failed");
        return NULL;
    }

    char *encoded = curl_easy_escape(curl, buffer, strlen(buffer));

    close(fd);
    munmap(buffer, size);

    return encoded;
}

int send_request(char *filename)
{
    CURL *curl = curl_easy_init();
    if (!curl) {
        puts("cannot init curl");
        return -1;
    }

    char *encoded = url_encoded_string(curl, filename);
    if (!encoded) {
        puts("cannot encode string");
        return -1;
    }

    char *request = generate_request(encoded, filename);
    curl_free(encoded);

    if (!request) {
        puts("cannot generate request");
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://pastebin.com/api/api_post.php");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "pastebin 1.0");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request);
    /* curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); */

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        printf("curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int res = 0;
    char *filename;

    if (argc == 1) {
        puts("usage: pastebin -f <filename>");
        return 0;
    }

    while ((res = getopt(argc, argv, "f:h")) != -1) {
        switch (res) {
            case 'f':
                filename = optarg;
            break;

            case '?':
                puts("usage: pastebin -f <filename>");
            break;
        };
    };

    if (send_request(filename) < 0) {
        puts("cannot send request");
        return -1;
    }

    return 0;
}
