#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <regex.h>

bool stop = false;

void my_alarm(int var)
{
	stop = true;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("usage: alarmclock time(hours:minutes)\n");
		return -1;
	}

    time_t rawtime;
    struct tm *timeinfo;
	int res, hours, w_hours, minutes, w_minutes, seconds;
    regex_t regex;
    const char *match = argv[1];

    res = regcomp(&regex, "^(2[0-3]|[01]?[0-9]):([0-5]?[0-9])$", REG_EXTENDED);
    if (res) {
        fprintf(stderr, "Could not compile regex\n");
        return -1;
    }

    res = regexec(&regex, match, 0, NULL, 0);

	if (res == REG_NOMATCH) {
        printf("invalid time\n");
        return -1;
    }
    regfree(&regex);

    sscanf(match, "%d:%d", &hours, &minutes);

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    w_hours = hours-timeinfo->tm_hour;
    w_minutes = minutes - timeinfo->tm_min;

    seconds = w_hours * 3600 + w_minutes * 60;

    if (seconds <= 0) {
    	printf("incorrect time\n");
    	return -1;
    }
    printf("alarm in %d hours %d minutes\n", w_hours < 0 ? 0 : w_hours, w_minutes < 0 ? 0 : w_minutes);

	signal(SIGALRM, my_alarm);
	alarm(seconds);

	while(!stop) {}

	printf("Knock-knock, Neo\n");

	return 0;
}