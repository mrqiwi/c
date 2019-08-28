/*tiny shell*/
/*gcc tiny_shell.c -o tiny_shell -lreadline */

#define _GNU_SOURCE     // for asprintf
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>


#define NRM  "\x1B[0m"
#define RED  "\x1B[31m"
#define GRN  "\x1B[32m"
#define YEL  "\x1B[33m"
#define BLU  "\x1B[34m"
#define MAG  "\x1B[35m"
#define CYN  "\x1B[36m"
#define WHT  "\x1B[37m"

#define BUFSIZE 64
#define DELIM " \t\r\n"

#define HISTORY_PATH "history.txt"

typedef struct {
  char *name;
  int (*func)(char**);
} func_t;

int tsh_cd(char **args)
{
    int res;

    //если не указана директория то переходим в домашнюю
    if (args[1] == NULL)
        res = chdir(getenv("HOME"));
    else
        res = chdir(args[1]);

    if (res < 0)
        fprintf(stderr, "cannot change directory\n");

    return res;
}


int tsh_exit(char **args)
{
    exit(EXIT_SUCCESS);
}

func_t funcs_list[] = {{"cd",   &tsh_cd},
                       {"exit", &tsh_exit},
                       {NULL, NULL}};


int execute_cmd(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();

    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            fprintf(stderr, "unknown command\n");
            exit(EXIT_FAILURE);
        }
    } else if (pid < 0) {
        fprintf(stderr, "forking child failed\n");
        exit(EXIT_FAILURE);
    } else {
        do {
          wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}


int execute_line(char **args)
{
    func_t *func_obj;
    char *cmd = args[0];

    for (func_obj = funcs_list; func_obj->func; func_obj++)
        if (!strcmp(func_obj->name, cmd))
            return func_obj->func(args);

    return execute_cmd(args);
}


char **split_line(char *line)
{
    int bufsize = PATH_MAX;
    int position = 0;
    char **tokens = malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "memory allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, DELIM);

    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += BUFSIZE;
            tokens = realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "memory allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}


char *get_path(void)
{
    char *pwd = getcwd(NULL, NAME_MAX);
    char *home = getenv("HOME");
    char *prefix = NULL;

    if (strstr(pwd, home)) {
        prefix = &pwd[strlen(home)-1];
        *prefix = '~';
        return prefix;
    } else
        return pwd;
}


char *get_greeting(void)
{
    char host[HOST_NAME_MAX];
    char *user = getenv("USER");
    char *path = get_path();
    char *buf;

    gethostname(host, HOST_NAME_MAX);
    asprintf(&buf, GRN "%s@%s" BLU " %s " NRM, user, host, path);

    return buf;
}

int main(void)
{
    char *line = NULL, *greeting = NULL;
    char **args;

    // rl_bind_key('\t', rl_complete);
    read_history(HISTORY_PATH);

    while (1) {
        greeting = get_greeting();
        line = readline(greeting);
        free(greeting);

        if (line && *line) {
            add_history(line);
            args = split_line(line);
            execute_line(args);
            write_history(HISTORY_PATH);

            free(line);
            free(args);
        }
    }

    return 0;
}

