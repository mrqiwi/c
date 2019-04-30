#ifndef _ROUTINES_H_
#define _ROUTINES_H_

#define MSH_RL_BUFSIZE 1024
#define MSH_TOK_BUFSIZE 64
#define MSH_TOK_DELIM " \t\r\n\a"
#define LEN 1024

#define NORMAL   "\x1B[0m"
#define RED      "\x1B[31m"
#define GREEN    "\x1B[32m"
#define YELLOW   "\x1B[33m"
#define BLUE     "\x1B[34m"
#define MAGENTA  "\x1B[35m"
#define CYAN     "\x1B[36m"
#define WHITE    "\x1B[37m"


typedef struct {
  char *name;
  int (*func)(char**);
} mshfunc_t;


#endif
