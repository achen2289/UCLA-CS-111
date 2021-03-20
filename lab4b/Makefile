# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047

.PHONY: default dist clean
.SILENT:

CC = gcc
CFLAGS = -Wall -Wextra -g -lm -lmraa

default: lab4b

lab4b: lab4b.c
	$(CC) $(CFLAGS) -o lab4b lab4b.c

check: lab4b
	{ echo "START"; sleep 1; echo "PERIOD=2"; echo "SCALE=C"; sleep 2; echo "STOP"; sleep 3; echo "OFF"; } | ./lab4b --log=logfile.txt; \

	if [ $$? -eq 0 ]; then \
		echo "TEST #1 PASSED: exit status of 0 when provided expected input"; \
	else \
		echo "TEST #1 FAILED: incorrect exit status when provided expected input"; \
	fi; \
	
	grep START logfile.txt; \
	if [ $$? -eq 0 ]; then \
		echo "TEST #2 PASSED: START command logged"; \
	else \
		echo "TEST #2 FAILED: START commmand not logged"; \
	fi; \
	
	grep STOP logfile.txt; \
	if [ $$? -eq 0 ]; then \
		echo "TEST #3 PASSED: STOP command logged"; \
	else \
		echo "TEST #3 FAILED: STOP command not logged"; \
	fi; \
	
	grep OFF logfile.txt; \
	if [ $$? -eq 0 ]; then \
		echo "TEST #4 PASSED: OFF command logged"; \
	else \
		echo "TEST #4 FAILED: OFF command not logged"; \
	fi; \
	
	grep SHUTDOWN logfile.txt; \
	if [ $$? -eq 0 ]; then \
		echo "TEST #5 PASSED: SHUTDOWN command logged"; \
	else \
		echo "TEST #5 FAILED: SHUTDOWN command not logged"; \
	fi; \
	
	rm -f logfile.txt

dist:
	tar -czvf lab4b-005299047.tar.gz lab4b.c Makefile README

clean:
	rm -f *.tar.gz lab4b