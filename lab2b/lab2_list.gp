#! /usr/bin/gnuplot
# NAME: Alex Chen
# EMAIL: achen2289@gmail.com
# ID: 005299047
#
# purpose:
#	 generate data reduction graphs for the multi-threaded list project
#
# input: lab2b_list.csv
#	1. test name
#	2. # threads
#	3. # iterations per thread
#	4. # lists
#	5. # operations performed (threads x iterations x (ins + lookup + delete))
#	6. run time (ns)
#	7. run time per operation (ns)
#    8. wait time per lock (ns)
#
# output:
#	lab2b_1.png ... throughput vs iterations
#
# Note:
#	Managing data is simplified by keeping all of the results in a single
#	file.  But this means that the individual graphing commands have to
#	grep to select only the data they want.
#
#	Early in your implementation, you will not have data for all of the
#	tests, and the later sections may generate errors for missing data.
#

# general plot parameters
set terminal png
set datafile separator ","

# lab2b_1.png
set title "List-1: Throughput vs Number of threads"
set xlabel "Number of threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (operations/second)"
set logscale y 10
set output 'lab2b_1.png'

# grep out only mutex and spin-lock synchronized results
plot \
     "< grep 'list-none-m,[0-9]*,1000,1' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list w/ mutex lock' with linespoints lc rgb 'red', \
     "< grep 'list-none-s,[0-9]*,1000,1' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title 'list w/ spin lock' with linespoints lc rgb 'green'

# lab2b_2.png
set title "List-2: Average wait for lock time and time per operation vs Number of threads"
set xlabel "Number of threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Time (ns)"
set logscale y 10
set output 'lab2b_2.png'

# grep out only wait time per lock acquisition and time per operation
plot \
     "< grep 'list-none-m,[0-9]*,1000,1' lab2b_list.csv" using ($2):($8) \
	title 'wait-for-lock' with linespoints lc rgb 'red', \
     "< grep 'list-none-m,[0-9]*,1000,1' lab2b_list.csv" using ($2):($7) \
	title 'time per operation' with linespoints lc rgb 'green'

# lab2b_3.png
set title "List-3: Partitioning shared resource, with and without synchronization"
set xlabel "Number of threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Successful iterations"
set logscale y 10
set output 'lab2b_3.png'

# grep out successful runs using 4 lists, mutex and spin-lock syncs, and 4 lists
plot \
     "< grep 'list-id-none,[0-9]*,[0-9]*,4' lab2b_list.csv" using ($2):($3) \
	title 'no synchronization' with points lc rgb 'green', \
     "< grep 'list-id-m,[0-9]*,[0-9]*,4' lab2b_list.csv" using ($2):($3) \
	title 'mutex' with points lc rgb 'red', \
     "< grep 'list-id-s,[0-9]*,[0-9]*,4' lab2b_list.csv" using ($2):($3) \
	title 'spin-lock' with points lc rgb 'blue'

# lab2b_4.png
set title "List-4: Throughput vs number of threads, mutex sync"
set xlabel "Number of threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (operations/second)"
set logscale y 10
set output 'lab2b_4.png'

# grep out mutex sync runs
plot \
     "< grep 'list-none-m,[0-9]*,1000,1' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '1 list' with linespoints lc rgb 'red', \
     "< grep 'list-none-m,[0-9]*,1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '4 lists' with linespoints lc rgb 'green', \
     "< grep 'list-none-m,[0-9]*,1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '8 lists' with linespoints lc rgb 'blue', \
     "< grep 'list-none-m,[0-9]*,1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '16 lists' with linespoints lc rgb 'yellow'

# lab2b_5.png
set title "List-4: Throughput vs number of threads, spin-lock sync"
set xlabel "Number of threads"
set logscale x 2
set xrange [0.75:]
set ylabel "Throughput (operations/second)"
set logscale y 10
set output 'lab2b_5.png'

# grep out mutex sync runs
plot \
     "< grep 'list-none-s,[0-9]*,1000,1' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '1 list' with linespoints lc rgb 'red', \
     "< grep 'list-none-s,[0-9]*,1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '4 lists' with linespoints lc rgb 'green', \
     "< grep 'list-none-s,[0-9]*,1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '8 lists' with linespoints lc rgb 'blue', \
     "< grep 'list-none-s,[0-9]*,1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
	title '16 lists' with linespoints lc rgb 'yellow'
