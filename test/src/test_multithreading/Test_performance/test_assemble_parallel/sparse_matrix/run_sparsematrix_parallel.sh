#!/bin/bash

#$1 nodi 
#$2 n thread
#$3 kk. n_job = kk*numero_worker (+1 se (numero_celle % num_worker*kk)!= 0)

output_file="test_sparsematrix_nodi"$1"_thread"$2".txt"
touch "$output_file"


./"test_sparsematrix_parallel" "$1" "$2" "$3" >> "$output_file" 
    


