#!/bin/bash

#$1 is the number of times the file is ran


#$2 size worker_queue
#$3 n_cicli in singolo job (ogni ciclo 4 operation)
#$4 n_job totali
#$5 sbilanciamento (cicli in job pesanti = sbilanciamnto* cicli in job leggeri)

#job eseguiti in single thread per poi confronto in speedup
#rm -f "test_job_single_thread_sbilanciato.txt"  #cosi add ogni volta nuovi test
touch "test_job_single_thread_sbilanciato.txt"

# Esegui il programma N volte e salva l'output
for i in $(seq 1 "$1"); do
    ./"test_job_single_thread_sbilanciato" "1" "$2" "$3" "$4" "$5">> "test_job_single_thread_sbilanciato.txt" 
done

# n_thread= []
threads=(6 4)
# Lista di eseguibili (modifica questa parte se serve)
executables=("test_sendMostfree_stealMostbusy_sbilanciato" "test_sendRound_stealMostbusy_sbilanciato" "test_sendMostfree_stealRandom_sbilanciato" "test_sendRound_stealRandom_sbilanciato")

for th in "${threads[@]}"; do
    # Per ogni eseguibile nella lista
    for exe in "${executables[@]}"; do
        # Crea (o sovrascrive) il file di output
        output_file="${exe}_numthread${th}.txt"
        #rm -f "$output_file"
        touch "$output_file"

        # Esegui il programma N volte e salva l'output
        for i in $(seq 1 "$1"); do
            ./"$exe" "$th" "$2" "$3" "$4" "$5">> "$output_file" 
        done
    done
done
