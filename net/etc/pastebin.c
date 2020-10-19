#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define PATTERN "api_option=paste" \
                "&api_user_key=" \
                "&api_paste_private=0"\
                "&api_paste_name=" \
                "&api_paste_expire_date=10M" \
                "&api_paste_format=" \
                "&api_dev_key=%s" \
                "&api_paste_code="

int main(void)
{
    char data[8192];
    char buffer[1024];
    char *dev_key = getenv("PASTEBIN");

    snprintf(data, sizeof(data), PATTERN, dev_key);
    //TODO разобраться с обрезанным буферомб уменьшить выхлоп curl
    while (fgets(buffer, sizeof(buffer), stdin))
        strcat(data, buffer);

    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://pastebin.com/api/api_post.php");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_NOBODY, 0);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return 0;
}
