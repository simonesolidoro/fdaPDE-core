#!/bin/bash

#$1 is the number of times the file is ran

#$2 size range of for
#$3 pesantezza body function (numero operazioni= $4 * 4)
#$4 is the number of threads, if needed
#$5 size queue of worker
#$6


# test_for.txt 
#rm -f "test_for.txt"
touch "test_for.txt"

# Esegui il programma N volte e salva l'output
for i in $(seq 1 "$1"); do
    ./"test_for" "$2" "$3" >> "test_for.txt" 
done

