#!/bin/bash

#$1 is the number of times the file is ran

#$2 size range of for
#$3 pesantezza body function (numero operazioni= $4 * 4)
#$4 is the number of threads, if needed
#$5 size queue of worker


# test_for.txt 
#rm -f "test_for.txt"
touch "test_for.txt"

# Esegui il programma N volte e salva l'output
for i in $(seq 1 "$1"); do
    ./"test_for" "$2" "$3" >> "test_for.txt" 
done

# n_blocchi = []
blocchi=(1 2 4 6 10 12 15 30 60)

# Per ogni eseguibile nella lista
for nb in "${blocchi[@]}"; do
    # Crea (o sovrascrive) il file di output
    output_file="test_parallel_for_sure_n_${nb}.txt"
    #rm -f "$output_file"
    touch "$output_file"

    # Esegui il programma N volte e salva l'output
    for i in $(seq 1 "$1"); do
        ./"test_parallel_for_sure_n" "$2" "$3" "$4" "$5" "$nb">> "$output_file" 
    done
done

