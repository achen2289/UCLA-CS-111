#!/bin/bash

for runs in 1
do
    for i in 2 4 8 12
    do
        for j in 10 20 40 80 100 1000 10000 100000
            do
                for sync_opt in m s c
                do
                    ./lab2_add --threads=$i --iterations=$j --yield --sync=$sync_opt
                done
            done
    done
done

for runs in 1
do
    for i in 2 4 6 8
    do
        for j in 100 1000 10000 100000
            do
                ./lab2_add --threads=$i --iterations=$j --yield
                ./lab2_add --threads=$i --iterations=$j
            done
    done
done

for runs in 1
do
    for j in 1 10 100 1000 10000 100000 1000000
    do
        ./lab2_add --iterations=$j
    done
done

for runs in 1
do
    for i in 2 4 8 12
    do
        ./lab2_add --threads=$i --iterations=10000 --sync=m --yield
        ./lab2_add --threads=$i --iterations=10000 --sync=c --yield
        ./lab2_add --threads=$i --iterations=1000 --sync=s --yield
    done
done

for runs in 1
do
    for i in 2 4 8 12
    do
        for j in 10 100 1000 10000 100000
        do
            ./lab2_add --threads=$i --iterations=$j
            for sync_opt in m s c
            do
                ./lab2_add --threads=$i --iterations=$j --sync=$sync_opt
            done
        done
    done
done

for runs in 1
do
    for i in 1 2 4 8 12
    do
        ./lab2_add --threads=$i --iterations=10000
        for sync_opt in m s c
        do
            ./lab2_add --threads=$i --iterations=10000 --sync=$sync_opt
        done
    done
done


    