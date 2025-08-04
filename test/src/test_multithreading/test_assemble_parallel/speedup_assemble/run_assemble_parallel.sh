#!/bin/bash

#$1 is the number of times the file is ran

#$2 nodi 
#$3 n thread
#$4 kk. n_job = kk*numero_worker (+1 se (numero_celle % num_worker*kk)!= 0)

output_file="test_assemble_nodi"$2"_thread"$3".txt"
touch "$output_file"

# Esegui il programma N volte e salva l'output
for i in $(seq 1 "$1"); do
    ./"test_assemble_parallel" "$2" "$3" "$4" >> "$output_file" 
    sleep 0.1  # evita congestione
done


