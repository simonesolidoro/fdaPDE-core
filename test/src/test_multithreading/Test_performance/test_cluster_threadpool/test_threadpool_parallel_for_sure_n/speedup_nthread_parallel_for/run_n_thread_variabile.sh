#!/bin/bash

#$1 is the number of times the file is ran

#$2 size range of for
#$3 pesantezza body function (numero operazioni= $4 * 4)
#$4 size queue of worker


# test_for.txt 
#rm -f "test_for.txt"
touch "test_for.txt"

# Esegui il programma N volte e salva l'output
for i in $(seq 1 "$1"); do
    ./"test_for" "$2" "$3" >> "test_for.txt" 
    sleep 0.1  # evita congestione
done

# n_blocchi = []
n_thread=(1 2 4 7 8)

# Per ogni eseguibile nella lista
for nt in "${n_thread[@]}"; do
    # Crea (o sovrascrive) il file di output
    output_file="test_parallel_for_sure_${nt}.txt"
    #rm -f "$output_file"
    touch "$output_file"

    # Esegui il programma N volte e salva l'output
    for i in $(seq 1 "$1"); do
        ./"test_parallel_for_sure" "$2" "$3" "$4" "$nt">> "$output_file" 
        sleep 0.5  # evita congestione
    done
done

