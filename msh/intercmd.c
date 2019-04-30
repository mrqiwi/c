#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "routines.h"


int msh_clear(char **args)
{
  fprintf(stdout, "\033[2J\033[0;0f");
  fflush(stdout);

  return 1;
}

int msh_cd(char **args)
{
  if (args[1] == NULL) {
    if (chdir(getenv("HOME")) != 0) {
      perror("msh");
    }
  } else {
    if (chdir(args[1]) != 0) {
      perror("msh");
    }
  }
  return 1;
}

int msh_help(char **args)
{
  const char *cmdlist[] = {"c", "cd", "clear", "exit", "help", NULL};
  const char **pcmd;

  printf("Cписок встроенных команд:\n");

  for (pcmd = cmdlist; *pcmd; pcmd++)
    printf("  %s\n", *pcmd);

  return 1;
}

int msh_exit(char **args)
{
  return 0;
}
