# This is a very simple make file which uses almost none of the power of "make".

CC=gcc
CFLAGS=-Wall

all: display test before after

display: display.c device.c
	$(CC) $(CFLAGS) -o display display.c device.c

test: main.c test.c CuTest.c fileSystem.c device.c
	$(CC) $(CFLAGS) -o test main.c test.c CuTest.c fileSystem.c device.c 

before: beforeTest.c fileSystem.c device.c 
	$(CC) $(CFLAGS) -o before beforeTest.c fileSystem.c device.c 

after: afterTest.c fileSystem.c device.c 
	$(CC) $(CFLAGS) -o after afterTest.c fileSystem.c device.c 

clean:
	rm display test before after
