#!/bin/bash

# Nome del file di output dove salvare tutti i risultati
output_file="risultati.txt"

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

#titolo
echo "pop come in threadpool(2thread) multithreading da back: 1 thread, singolo da front. di 16000 elementi " >> $output_file

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

echo "Worker_queue:  " >> $output_file
# Ciclo for per eseguire il programma più volte
for i in {1..100}; do
    # Esegui il programma e salva l'output nel file
    ./test_worker_queue $i >> $output_file
done

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

# Esegui il secondo file 50 volte
echo "deque:  " >> $output_file
for i in {1..100}; do
    ./test_deque $i >> $output_file
done

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

# Esegui il secondo file 50 volte
echo "old_worker_queue:  " >> $output_file
for i in {1..100}; do
    ./test_old_worker_queue $i >> $output_file
done

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

echo "Esecuzione completata. I risultati sono stati salvati in $output_file"

