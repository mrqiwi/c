#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/watchdog.h>

#define WATCHDOGDEV "/dev/watchdog"

static const char *const short_options = "hd:i:";
static const struct option long_options[] = {
    {"help", 0, NULL, 'h'},
    {"dev", 1, NULL, 'd'},
    {"interval", 1, NULL, 'i'},
    {NULL, 0, NULL, 0},
};

static void print_usage(char *app_name, int exit_code)
{
    printf("Usage: %s [options]\n", app_name);
    printf(
            " -h  --help                Display this usage information.\n"
            " -d  --dev <device_file>   Use <device_file> as watchdog device file.\n"
            "                           The default device file is '/dev/watchdog'\n"
            " -i  --interval <interval> Change the watchdog interval time\n");

    exit(exit_code);
}

int main(int argc, char **argv)
{
    char *dev = WATCHDOGDEV;
    int bootstatus, next_option, interval = 0;

    do {
        next_option = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (next_option) {
            case 'h':
                print_usage(argv[0], EXIT_SUCCESS);
            case 'd':
                dev = optarg;
                break;
            case 'i':
                interval = atoi(optarg);
                break;
            case '?':
                print_usage(argv[0], EXIT_FAILURE);
            case -1:
                break;
            default:
                abort();
        }
    } while (next_option != -1);

    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        perror("open()");
        exit(EXIT_FAILURE);
    }

    if (interval != 0) {
        printf("Set watchdog interval to %d\n", interval);
        if (ioctl(fd, WDIOC_SETTIMEOUT, &interval) != 0) {
            puts("Error: Set watchdog interval failed");
            exit(EXIT_FAILURE);
        }
    }

    if (ioctl(fd, WDIOC_GETTIMEOUT, &interval) == 0) {
        printf("Current watchdog interval is %d\n", interval);
    } else {
        puts("Error: Cannot read watchdog interval");
        exit(EXIT_FAILURE);
    }

    if (ioctl(fd, WDIOC_GETBOOTSTATUS, &bootstatus) == 0) {
        printf("Last boot is caused by : %s\n", (bootstatus != 0) ? "Watchdog" : "Power-On-Reset");
    } else {
        puts("Error: Cannot read watchdog status");
        exit(EXIT_FAILURE);
    }

    /* The 'V' value needs to be written into watchdog device file to indicate
     * that we intend to close/stop the watchdog. Otherwise, debug message
     * 'Watchdog timer closed unexpectedly' will be printed
     */
    write(fd, "V", 1);
    close(fd);
}
