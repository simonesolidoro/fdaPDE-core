#!/bin/bash

n_threads=(1 2 3 4 5 6 7 8) # 1 lasciato per vedere overhead threadpool rispetto seq
output_file="test_sq_Opt_.txt"
for i in "${n_threads[@]}"; do
    ./test_sq "$i" >> "$output_file"
done
