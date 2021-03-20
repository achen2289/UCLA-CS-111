# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

CC = gcc
CFLAGS = -Wall -Wextra -g

default: lab0

lab0:
	$(CC) $(CFLAGS) -o lab0 lab0.c

check: lab0
	chmod a+x smoke_test.sh
	./smoke_test.sh

dist:
	tar -czvf lab0-005299047.tar.gz lab0.c backtrace.png breakpoint.png smoke_test.sh Makefile README

clean: 
	rm -f lab0 *.txt *.tar.gz