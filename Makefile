CC=gcc 
CFLAGS=-Wall

all : simple_server

simple_server : main.o
	gcc -o simple_server main.o 

main.o : main.cpp
	gcc -c main.cpp

clean:
	rm -f simple_server

run: simple_server
	./simple_server
