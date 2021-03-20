# !/bin/bash

for i in 1 2 4 8 12 16 24
do
    ./lab2_list --threads=$i --iterations=1000 --sync=m
    ./lab2_list --threads=$i --iterations=1000 --sync=s
done

for i in 1 4 8 12 16
do
    for j in 1 2 4 8 16
    do
        ./lab2_list --threads=$i --iterations=$j --yield=id --lists=4
    done
done

for i in 1 4 8 12 16
do
    for j in 10 20 40 80
    do
        ./lab2_list --threads=$i --iterations=$j --yield=id --lists=4 --sync=m
        ./lab2_list --threads=$i --iterations=$j --yield=id --lists=4 --sync=s
    done
done

for i in 1 2 4 8 12
do
    for k in 1 4 8 16
    do
        ./lab2_list --threads=$i --iterations=1000 --lists=$k --sync=m
        ./lab2_list --threads=$i --iterations=1000 --lists=$k --sync=s
    done
done