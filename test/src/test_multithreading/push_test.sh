#!/bin/bash

# Nome del file di output dove salvare tutti i risultati
output_file="worker_queue_vs_deque.txt"

# per file vuoto
> $output_file

# Ciclo for per eseguire il programma più volte
for i in {1..5}; do
    # Esegui il programma e salva l'output nel file
    ./test_deque_vs_worker_queue $i >> $output_file
done

echo "Esecuzione completata. I risultati sono stati salvati in $output_file"
