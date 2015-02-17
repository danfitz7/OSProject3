#Start of the Makefile
all: problem2.o
	gcc -std=c99 -pthread -o problem2 problem2.o -I.

problem2.o: problem2.c
	gcc -std=c99 -Wall -g -c problem2.c -I.

clean:
	rm *.o problem2

##End of the Makefile