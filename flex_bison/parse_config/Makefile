parse: parse.y parse.l
	bison -d parse.y
	flex  -o parse.lex.c parse.l
	gcc -Wall -o parse parse.lex.c parse.tab.c -lfl

clean:
	rm -rf parse *.c *.h
