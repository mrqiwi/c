binary: main.o libworld.a
	gcc -o binary main.o -L. -lworld

main.o: main.c
	gcc -c main.c

libworld.a: world.o
	ar cr libworld.a world.o

world.o: world.c
	gcc -c world.c

clean:
	rm -f *.o *.a binary
