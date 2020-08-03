#include <stdio.h>

#define COLOR_RED           "\x1b[31m"
#define COLOR_GREEN         "\x1b[32m"
#define COLOR_YELLOW        "\x1b[33m"
#define COLOR_MAGENTA       "\x1b[35m"
#define COLOR_CYAN          "\x1b[36m"
#define COLOR_WHITE         "\x1b[97m"
#define COLOR_RESET         "\x1b[0m"

#define red(text)           COLOR_RED text COLOR_RESET
#define green(text)         COLOR_GREEN text COLOR_RESET
#define yellow(text)        COLOR_YELLOW text COLOR_RESET
#define magenta(text)       COLOR_MAGENTA text COLOR_RESET
#define cyan(text)          COLOR_CYAN text COLOR_RESET
#define white(text)         COLOR_WHITE text COLOR_RESET

#define error_msg(msg, ...)  fprintf(stdout, red(msg "\n"), ##__VA_ARGS__)
#define warn_msg(msg, ...)   fprintf(stdout, yellow(msg "\n"), ##__VA_ARGS__)
#define info_msg(msg, ...)   fprintf(stdout, green(msg "\n"), ##__VA_ARGS__)

int main(void)
{
    error_msg("error = %s(%d)", "haha", 5);
    warn_msg("error = %s(%d)", "haha", 6);
    info_msg("error = %s(%d)", "haha", 2);

    return 0;
}
