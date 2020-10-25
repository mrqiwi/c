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

static char *generate_request(char *filename)
{
    char *filetype = strrchr(filename, '.');
    if (filetype)
        filetype++;
    else
        filetype = "";

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

    int length = snprintf(NULL, 0, PATTERN, filetype, getenv("PASTEBIN"), buffer) + 1;

    char *data = malloc(length);
    if (!data) {
        close(fd);
        munmap(buffer, size);
        perror("malloc() failed");
        return NULL;
    }

    snprintf(data, length, PATTERN, filetype, getenv("PASTEBIN"), buffer);

    close(fd);
    munmap(buffer, size);

    return data;
}

int send_request(char *data)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return -1;

    curl_easy_setopt(curl, CURLOPT_URL, "https://pastebin.com/api/api_post.php");
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    //NOTE некорректный размер Content-Lenght, не понятно почему
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, strlen(data));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
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

    char *data = generate_request(filename);
    if (data == NULL) {
        puts("cannot generate request");
        return -1;
    }

    res = send_request(data);
    free(data);

    if (res < 0) {
        puts("cannot send request");
        return -1;
    }

    return 0;
}
