#!/bin/bash

#$1 is the number of times the file is ran

#$2 nodi 
#$3 n thread
#$4 kk. n_job = kk*numero_worker (+1 se (numero_celle % num_worker*kk)!= 0)

output_file="test_calcolo_triple_nodi"$2"_thread"$3".txt"
touch "$output_file"


./"test_calcolo_triple_parallel" "$2" "$3" "$4" >> "$output_file" 
    


