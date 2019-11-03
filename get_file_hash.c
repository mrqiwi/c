// gcc -o file file.c -lssl -lcrypto
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <openssl/md5.h>

int main(void)
{
    unsigned char c[MD5_DIGEST_LENGTH], data[1024];
    int i, bytes;
    MD5_CTX mdContext;
    char *filename = "file.c";
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL) {
        printf ("%s can't be opened.\n", filename);
        return 0;
    }

    MD5_Init(&mdContext);

    while ((bytes = fread(data, 1, 1024, fp)) != 0)
        MD5_Update(&mdContext, data, bytes);

    MD5_Final(c, &mdContext);

    for (i = 0; i < MD5_DIGEST_LENGTH; i++)
        printf("%02x", c[i]);

    printf(" %s\n", filename);
    fclose(fp);

    return 0;
}
