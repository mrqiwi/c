#include <stdio.h>
#include <string.h>
#include <glob.h>

int main(void)
{
	glob_t paths;

	memset(&paths, 0, sizeof(glob_t));

	glob("/home/sergey/scrpits/*.py", 0, NULL, &paths);

	for (int i = 0; i < paths.gl_pathc; i++)
		printf( "[%d]: %s\n", i+1, paths.gl_pathv[i]);

	globfree(&paths);

	return 0;
}