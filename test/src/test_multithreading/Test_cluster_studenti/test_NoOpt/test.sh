#!/bin/bash

n_threads=(1 2 4 8) # 1 lasciato per vedere overhead threadpool rispetto seq 
output_file="test_noOpt_.txt"
for i in "${n_threads[@]}"; do
    ./test "$i" >> "$output_file"
done
