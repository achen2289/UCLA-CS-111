# !/bin/bash

for j in 10 100 1000 10000 20000
do
    ./lab2_list --threads=1 --iterations=$j 
done

for i in 2 4 8 12
do
    for j in 1 10 100 1000
    do
        ./lab2_list --threads=$i --iterations=$j
    done
done

for i in 2 4 8 12
do
    for j in 1 2 4 8 16 32
    do
        for yield_opt in i d l id il dl idl
        do
            ./lab2_list --threads=$i --iterations=$j --yield=$yield_opt
        done
    done
done

for i in 2 4 8 12
do
    for j in 1 2 4 8 16 32
    do
        for yield_opt in i d l id il dl idl
        do
            ./lab2_list --threads=$i --iterations=$j --yield=$yield_opt --sync=m
            ./lab2_list --threads=$i --iterations=$j --yield=$yield_opt --sync=s
        done
    done
done

for i in 1 2 4 8 12 16 24
do
    for yield_opt in i d l id il dl idl
    do
        ./lab2_list --threads=$i --iterations=1000 --sync=m
        ./lab2_list --threads=$i --iterations=1000 --sync=s
    done
done