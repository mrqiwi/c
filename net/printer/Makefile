PROGS = print printd
HDRS = print.h ipp.h
LIBS=-pthread

all: $(PROGS)

util.o: util.c $(HDRS)

print.o: print.c $(HDRS)

printd.o: printd.c $(HDRS)

print: print.o util.o
	gcc -o print print.o util.o

printd: printd.o util.o
	gcc -o printd printd.o util.o $(LIBS)

clean:
	rm -f $(PROGS) *.o
