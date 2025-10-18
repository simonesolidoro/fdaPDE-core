#!/bin/bash

#$1 nodi 

output_file="test_sparsematrix_nodi"$1".txt"
touch "$output_file"

./"test_sparsematrix" "$1" >> "$output_file" 

