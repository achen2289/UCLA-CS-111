# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

.PHONY: default dist clean
.SILENT: 

CC = gcc
CFLAGS = -Wall -Wextra

default: lab3a

lab3a: lab3a.c
	$(CC) $(CFLAGS) -o lab3a lab3a.c

dist:
	tar -czvf lab3a-005299047.tar.gz lab3a.c ext2_fs.h Makefile README

clean:
	rm -f lab3a *.tar.gz