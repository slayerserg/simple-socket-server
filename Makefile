all : select-server poll-server client

select-server : select-server.o
	$(GCC) -o select-server select-server.o 

select-server.o : select-server.cpp
	$(GCC) -c select-server.cpp

poll-server : poll-server.o
	$(GCC) -o poll-server poll-server.o 

poll-server.o : poll-server.cpp
	$(GCC) -c poll-server.cpp

client : client.o
	$(GCC) -o client client.o 

client.o : client.cpp
	$(GCC) -c client.cpp

clean:
	rm -f select-server select-server.o client client.o

run: select-server
	./select-server
