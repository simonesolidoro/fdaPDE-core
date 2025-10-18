#!/bin/bash

#$1 is the number of times the file is ran

#$2 size worker_queue
#$3 n_cicli in singolo job (ogni ciclo 4 operation)
#$4 n_job totali
#$5 _thread (uo per volta perche a computer uico .sh co for affatica)

# n_thread= []
threads=($5)

######## uno e meta###################
executables=("test_Mostbusy_unometa" "test_Random_unometa")

# Prepara i file di output vuoti all'inizio
for th in "${threads[@]}"; do
    for exe in "${executables[@]}"; do
        output_file="${exe}_numthread${th}.txt"
        touch "$output_file"
    done
done

# Loop principale: run mischiati
for run in $(seq 1 "$1"); do
    for th in "${threads[@]}"; do
        for exe in "${executables[@]}"; do
            output_file="${exe}_numthread${th}.txt"
            ./"$exe" "$th" "$2" "$3" "$4" >> "$output_file"
            sleep 0.1  # evita congestione
        done
    done
done
