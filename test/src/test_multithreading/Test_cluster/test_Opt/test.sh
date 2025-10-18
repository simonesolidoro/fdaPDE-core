#!/bin/bash

n_threads=(2 4)

for i in "${n_threads[@]}"; do
    output_file="test_thread_${i}_.txt"
    ./test "$i" >> "$output_file"
done
