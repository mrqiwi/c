#include <stdio.h>
#include <unistd.h>

int main(void)
{
    const char *progress = "-\\|/";

    for (const char *ptr = progress;; ptr++) {
        if (*ptr == '\0')
            ptr = progress;
        printf("\r%c progress...", *ptr);
        usleep(500000);
        fflush(stdout);
    }

    return 0;
}