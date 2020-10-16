%{
#include <stdio.h>
#include <string.h>

#define YYSTYPE char *

void yyerror(const char *str)
{
        fprintf(stderr, "error: %s\n", str);
}

int yywrap()
{
        return 1;
}

main()
{
        yyparse();
}

%}

%token ZONETOK FILETOK WORD FILENAME QUOTE OBRACE EBRACE SEMICOLON
%%

commands:
        |
        commands command SEMICOLON
        ;


command:
       zone_set
        ;

zone_set:
        ZONETOK quotedname zonecontent
        {
                printf("Найдена полная зона для '%s'\n", $2);
        }
        ;

zonecontent:
           OBRACE zonestatements EBRACE

quotedname:
          QUOTE FILENAME QUOTE
        {
                $$=$2;
        }

zonestatements:
              |
        zonestatements zonestatement SEMICOLON
        ;

zonestatement:
             statements
        |
        FILETOK quotedname
        {
                printf("Обнаружено имя файла зоны '%s'\n", $2);
        }
        ;
block:
     OBRACE zonestatements EBRACE SEMICOLON
        ;

statements:
          | statements statement
        ;

statement: WORD | block | quotedname
