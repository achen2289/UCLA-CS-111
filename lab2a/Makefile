# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

CC = gcc
CFLAGS = -Wall -Wextra -pthread

default: build

build: lab2_add lab2_list

lab2_add: lab2_add.c
	$(CC) $(CFLAGS) -o lab2_add lab2_add.c 

lab2_list: lab2_list.c
	$(CC) $(CFLAGS) -o lab2_list lab2_list.c SortedList.c

tests: lab2_add lab2_list 
	rm -f lab2_add.csv lab2_list.csv
	chmod +x test_add.sh
	chmod +x test_list.sh
	./test_add.sh
	-./test_list.sh 2>/dev/null

graphs: lab2_add.gp lab2_list.gp tests
	chmod +x lab2_add.gp
	chmod +x lab2_list.gp
	./lab2_add.gp
	./lab2_list.gp

dist: graphs
	tar -czvf lab2a-005299047.tar.gz lab2_add.c lab2_list.c SortedList.h SortedList.c  \
	lab2_add.csv lab2_list.csv lab2_add-1.png lab2_add-2.png lab2_add-3.png lab2_add-4.png \
	lab2_add-5.png lab2_list-1.png lab2_list-2.png lab2_list-3.png lab2_list-4.png \
	lab2_add.gp lab2_list.gp test_add.sh test_list.sh Makefile README

clean:
	rm -f lab2_add lab2_list *.tar.gz *.txt
