CC := gcc-13
CFLAGS := -Wall -Wextra -pedantic -xc -std=c99

.PHONY: all test_dir clean


all: vector.a


vector.a: vector.o
	ar ruv vector.a vector.o
	ranlib vector.a

vector.o: vector.c vector.h
	$(CC) $(CFLAGS) -O2 -c vector.c -o vector.o


test: test_dir test/test.o
	$(CC) test/test.o -o test/test

user-test: test_dir test/user-test.o vector.o
	$(CC) test/user-test.o vector.o -o test/user-test

test_dir:
	mkdir -p test

test/test.o: test.c vector.c vector.h
	$(CC) $(CFLAGS) -g -c test.c -o test/test.o

test/user-test.o: user-test.c vector.h
	$(CC) $(CFLAGS) -g -c user-test.c -o test/user-test.o


clean:
	rm -rf vector.a vector.o test/
