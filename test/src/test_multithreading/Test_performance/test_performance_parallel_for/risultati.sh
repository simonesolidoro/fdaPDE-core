#!/bin/bash

#$1 is the name of the file to be ran
#$2 is the number of times the file is ran

#$3 size range of for
#$4 pesantezza body function (numero operazioni= $4 * 4)
#$5 is the number of threads, if needed
#$6 size queue of worker
#$7 n of parallel_for_sure_n (numero blocchi in cui divide range di for iniziale)


rm $1".txt"

touch $1".txt"
for i in `seq 1 $2`; do
    # Esegui il programma e salva l'output nel file
    ./$1 $3 $4 $5 $6 $7 >>$1".txt"
done
