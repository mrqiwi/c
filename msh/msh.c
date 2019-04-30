#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "routines.h"
#include "intercmd.h"

 mshfunc_t funcs_list[] = {{"cd",   &msh_cd},
                          {"exit", &msh_exit},
                          {"clear", &msh_clear},
                          {"c", &msh_clear},
                          {"help", &msh_help},
                          {NULL, NULL}};

void msh_save_history(char **cmd)
{
  FILE *fp;
  char **p = cmd;
  char path[LEN];
  int cnt = 0;

  snprintf(path, sizeof(path), "%s/.msh_history", getenv("HOME"));

  fp = fopen(path, "a+");

  if (!fp)
    perror("msh history");

  while (!feof(fp)) {
    if (fgetc(fp) == '\n')
     cnt++;
  }
  /*номер строки*/
  fprintf(fp, "%d\t", cnt);

  /*по частям записываем команду*/
  while (*p)
    fprintf(fp, "%s ", *p++);

  /*конец строки*/
  fprintf(fp, "\n");

  fclose(fp);
}

char *msh_read_line(void)
{
  int bufsize = MSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;

  if (!buffer) {
    fprintf(stderr, "msh: ошибка выделения памяти\n");
    exit(EXIT_FAILURE);
  }

  while (1) {
    c = getchar();

    /*если первый символ - конец ввода то выходим*/
    if (c == EOF && position == 0) {
      exit(EXIT_SUCCESS);
    } else if (c == EOF || c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {

      buffer[position] = c;
    }
    position++;

    if (position >= bufsize) {
      bufsize += MSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer) {
        fprintf(stderr, "msh: ошибка выделения памяти\n");
        exit(EXIT_FAILURE);
      }
    }

  }
}

char **msh_split_line(char *line)
{
  int bufsize = MSH_TOK_BUFSIZE;
  int position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token;

  if (!tokens) {
    fprintf(stderr, "msh: ошибка выделения памяти\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, MSH_TOK_DELIM);
  while (token != NULL) {
    tokens[position] = token;
    position++;

    if (position >= bufsize) {
      bufsize += MSH_TOK_BUFSIZE;
      tokens = realloc(tokens, bufsize * sizeof(char*));
      if (!tokens) {
        fprintf(stderr, "msh: ошибка выделения памяти\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, MSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

int msh_launch(char **args)
{
  pid_t pid, wpid;
  int status;

  pid = fork();
  if (pid == 0) {
    /*дочерний процесс*/
    if (execvp(args[0], args) == -1) {
      perror("msh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    /*ошибка при форкинге*/
    perror("msh");
  } else {
    /*родительский процесс*/
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  return 1;
}


int msh_execute(char **args)
{
  mshfunc_t *func_obj;
  struct stat statbuf;
  char *cmd = args[0];

  if (cmd == NULL)
    return 1;

  /*если это директория, то перейдем в нее*/
  if (stat(cmd, &statbuf) == 0)
    if (S_ISDIR(statbuf.st_mode))
      if (chdir(cmd) == 0)
        return 1;

  /*если встроенная команда, то выполним*/
  for (func_obj = funcs_list; func_obj->func; func_obj++)
    if (!strcmp(func_obj->name, cmd))
      return func_obj->func(args);

   /*ll - аналог ls -l*/
  if(!strcmp("ll", cmd) && args[1] == NULL) {
    args[0] = "ls";
    args[1] = "-l";
  }

  return msh_launch(args);
}

void show_info(void)
{
  char hostname[LEN], curdir[LEN];
  char *pcurdir;
  int pos = 0;

  gethostname(hostname, LEN-1);
  getcwd(curdir, LEN);

  if (strstr(curdir, getenv("HOME"))) {
    pos = strlen(curdir) - strlen(getenv("HOME"));
    curdir[strlen(curdir+1) - pos] = '~';
    pcurdir = &curdir[strlen(curdir+1) - pos];
  } else {
    pcurdir = curdir;
  }

  printf("%s%s@%s%s %s %s", CYAN, getenv("USER"), hostname, GREEN, pcurdir, NORMAL);
}


void msh_loop(void)
{
  char *line;
  char **args;
  int status;

  do {
    show_info();
    line = msh_read_line();
    args = msh_split_line(line);
    msh_save_history(args);
    status = msh_execute(args);

    free(line);
    free(args);
  } while (status);
}


int main(void)
{
  // Загрузка файлов конфигурации при их наличии.

  // Запуск цикла команд.
  msh_loop();

  // Выключение / очистка памяти.

  return -1;
}
