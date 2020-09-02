binary: main.o libworld.so
	gcc -o binary main.o -lworld

main.o: main.c
	sudo cp world.h /usr/local/include
	gcc -c main.c

libworld.so: world.o
	gcc -shared -o libworld.so world.o
	sudo mv libworld.so /usr/lib

world.o: world.c
	gcc -c -fPIC world.c

clean:
	rm -f *.o *.so binary
