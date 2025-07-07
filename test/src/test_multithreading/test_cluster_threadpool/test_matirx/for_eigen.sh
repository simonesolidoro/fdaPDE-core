#!/bin/bash

#$1 is the number of times the file is ran

#$2 size of nxn matrix


# test_for.txt 
#rm -f "test_for.txt"
touch "test_Eigen_for.txt"

# Esegui il programma N volte e salva l'output
for i in $(seq 1 "$1"); do
    ./"test_Eigen_for" "$2" >> "test_Eigen_for.txt" 
done
