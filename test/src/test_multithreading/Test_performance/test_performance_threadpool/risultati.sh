#!/bin/bash

#$1 is the name of the file to be ran
#$2 is the number of times the file is ran
#$3 is the number of operation in function
#$4 is the number of threads, if needed

rm $1".txt"

touch $1".txt"
for i in `seq 1 $2`; do
    # Esegui il programma e salva l'output nel file
    ./$1 $3 $4 >>$1".txt"
    sleep 0.1  # evita congestione
done


