#include <stdio.h>
#include <unistd.h>

int main(void)
{
	char progress[] = "-\\|/";
	int i = 0;

	while(1) {
		printf("\r%c waiting...", progress[i == 4 ? i = 0 : i]);
		usleep(500000);
       	fflush(stdout);
       	i++;
    }

   printf("\n");

   return 0;
}