# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

default: build

build: lab2_list

lab2_list: lab2_list.c
	$(CC) $(CFLAGS) -o lab2_list lab2_list.c SortedList.c

tests: lab2_list 
	rm -f lab2b_list.csv
	chmod +x test_list.sh
	-./test_list.sh 2>/dev/null

graphs: lab2_list.gp tests
	chmod +x lab2_list.gp
	./lab2_list.gp

profile: default
	rm -f ./raw.gperf profile.out 
	LD_PRELOAD=/usr/lib64/libprofiler.so CPUPROFILE=./raw.gperf ./lab2_list --threads=12 --iterations=1000 --sync=s
	pprof --text ./lab2_list ./raw.gperf > profile.out
	pprof --list=list_op ./lab2_list ./raw.gperf >> profile.out
	rm -f ./raw.gperf lab2_list

dist: graphs
	tar -czvf lab2b-005299047.tar.gz lab2_list.c SortedList.h SortedList.c  \
	lab2b_list.csv lab2b_1.png lab2b_2.png lab2b_3.png lab2b_4.png lab2b_5.png \
	profile.out test_list.sh lab2_list.gp \
	Makefile README

clean:
	rm -f lab2_list *.tar.gz *.txt
