#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

int writef(int fd, const char *format, ...)
{
    va_list tmp_args, args;
    char *buff;
    int len, res;

    va_start(tmp_args, format);

    va_copy(args, tmp_args);

    len = vsnprintf(NULL, 0, format, tmp_args);

    buff = malloc(len+1);

    if (!buff)
        return -1;

    vsprintf(buff, format, args);

	res = write(fd, buff, len);

    free(buff);

    va_end(args);

    return res;
}

int main(int argc, char const *argv[])
{
	if (argc != 2) {
		printf("no filename\n");
		return -1;
	}

    int fd = open(argv[1], O_CREAT | O_WRONLY | O_APPEND, 0644);

    if (fd < 0) {
    	printf("cannot open file %s\n", argv[1]);
        return -1;
    }

	const char *username = getenv("USER");

	if (!username) {
    	printf("USER is empty\n");
    	return -1;
	}

	writef(fd, "%s\n", username);

    return 0;
}




