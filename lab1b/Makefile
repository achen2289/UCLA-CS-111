# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

CC = gcc
CFLAGS = -Wall -Wextra -lz

default: client server

client: lab1b-client.c
	$(CC) $(CFLAGS) -o lab1b-client lab1b-client.c

server: lab1b-server.c
	$(CC) $(CFLAGS) -o lab1b-server lab1b-server.c

dist:
	tar -czvf lab1b-005299047.tar.gz lab1b-client.c lab1b-server.c Makefile README

clean:
	rm -f lab1b-client lab1b-server *.tar.gz *.txt