#include <stdio.h>
#include <string.h>

#define LEN 100

int parse_tag(char *, char *, char *);

int main(void)
{
    char buffer[LEN]= {0};
    char *tag = "BlogType";
    char *response ="<Response>\
                    <Name>aticleworld.com</Name>\
                    <year>1.5</year>\
                    <BlogType>Embedded c and c++</BlogType>\
                    <Author>amlendra</Author>\
                    </Response>";

    if (parse_tag(response, tag, buffer) < 0)
        return -1;

    printf("%s",buffer);

    return 0;
}

int parse_tag(char *response, char *tag, char *buffer)
{
    int len = 0, pos = 0;
    int first_pos = 0, last_pos = 0;
    char first_tag[LEN] = {0}, second_tag[LEN] = {0};

    len = strlen(response);

    if (len <= 0)
        return -1;

    snprintf(first_tag, sizeof(first_tag), "<%s>", tag);
    snprintf(second_tag, sizeof(second_tag), "</%s>", tag);

    while (pos++ < len)
        if (memcmp(first_tag, response+pos, strlen(first_tag)) == 0) {
            first_pos = pos + strlen(first_tag);
            break;
        }

    while (pos++ < len)
        if (memcmp(second_tag, response+pos, strlen(second_tag)) == 0){
            last_pos = pos;
            break;
        }

    memcpy(buffer, response+first_pos, last_pos-first_pos);

    if (strlen(buffer))
        return 1;

    return -1;
}
