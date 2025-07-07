#!/bin/bash

#$1 is the number of times the file is ran

#$2 size of nxn matrix
#$3 granularity (e quindi size/gran = numero di thread) serve sia divisore di size !!
#$4 n_thread

granularity=($3)

# Per ogni eseguibile nella lista
for nb in "${granularity[@]}"; do
    # Crea (o sovrascrive) il file di output
    output_file="test_for_threads_gr${nb}.txt"
    #rm -f "$output_file"
    touch "$output_file"

# Esegui il programma N volte e salva l'output
    for i in $(seq 1 "$1"); do
        ./"test_for_threads" "$2" "$nb" "$4" >> "test_for_threads_gr${nb}.txt" 
        sleep 0.1
    done
done