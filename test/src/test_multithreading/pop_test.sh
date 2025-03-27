#!/bin/bash

# Nome del file di output dove salvare tutti i risultati
output_file="pop_worker_queue_vs_deque_vs_old_worker.txt"

# per file vuoto
#> $output_file

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

echo "Worker_queue:  pop di 1600 elemnti con 2 thread:" >> $output_file
# Ciclo for per eseguire il programma più volte
for i in {1..50}; do
    # Esegui il programma e salva l'output nel file
    ./test_worker_queue $i >> $output_file
done

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

# Esegui il secondo file 50 volte
echo "deque:  pop di 1600 elemnti con 2 thread:" >> $output_file
for i in {1..50}; do
    ./test_deque $i >> $output_file
done

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

# Esegui il secondo file 50 volte
echo "old_worker_queue:  pop di 1600 elemnti con 2 thread:" >> $output_file
for i in {1..50}; do
    ./test_old_worker_queue $i >> $output_file
done

echo "Esecuzione completata. I risultati sono stati salvati in $output_file"

