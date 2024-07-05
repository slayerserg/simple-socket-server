CC=g++
CFLAGS=-Wall

all : simple_server

simple_server : select-server.o
	g++ -o simple_server select-server.o 

select-server.o : select-server.cpp
	g++ -c select-server.cpp

clean:
	rm -f simple_server

run: simple_server
	./simple_server
