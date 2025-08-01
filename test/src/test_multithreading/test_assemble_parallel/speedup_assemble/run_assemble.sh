#!/bin/bash

#$1 is the number of times the file is ran

#$2 nodi 

output_file="test_assemble_nodi"$2".txt"
touch "$output_file"

# Esegui il programma N volte e salva l'output
for i in $(seq 1 "$1"); do
    ./"test_assemble" "$2" >> "$output_file" 
    sleep 0.1  # evita congestione
done

