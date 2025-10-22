#!/bin/bash

n_threads=(32 64) # 1 lasciato per vedere overhead threadpool rispetto seq

for i in "${n_threads[@]}"; do
    output_file="test_Opt_thread_${i}_.txt"
    ./test "$i" >> "$output_file"
done
