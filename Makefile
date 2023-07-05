CC := gcc-13
CFLAGS := -Wall -Wextra -pedantic -xc -std=c11

.PHONY: all test test_dir clean


all: vector.a


vector.a: vector.o
	ar ruv vector.a vector.o
	ranlib vector.a

vector.o: vector.c vector.h
	$(CC) $(CFLAGS) -O2 -c vector.c -o vector.o


test: test_dir test/test.o
	$(CC) test/test.o -o test/test

test_dir:
	mkdir -p test

test/test.o: test.c vector.c vector.h
	$(CC) $(CFLAGS) -g -c test.c -o test/test.o


clean:
	rm -rf vector.a vector.o test/
