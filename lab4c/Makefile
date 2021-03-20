# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

.PHONY: default dist clean

CC = gcc
CFLAGS = -Wall -Wextra -g -lm -lmraa

default: lab4c_tcp lab4c_tls

lab4c_tcp: lab4c_tcp.c
	$(CC) $(CFLAGS) -o lab4c_tcp lab4c_tcp.c

lab4c_tls: lab4c_tls.c
	$(CC) $(CFLAGS) -lssl -lcrypto -o lab4c_tls lab4c_tls.c

dist:
	tar -czvf lab4c-005299047.tar.gz lab4c_tcp.c lab4c_tls.c Makefile README

clean:
	rm -f *.tar.gz lab4c_tcp lab4c_tls