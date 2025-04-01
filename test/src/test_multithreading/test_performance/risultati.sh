#!/bin/bash

# Nome del file di output dove salvare tutti i risultati
output_file="prova.txt"

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

#titolo
echo "pop_front() di 16000 elementi" >> $output_file

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file


# Esegui il secondo file 50 volte
echo "Worker_queue:  " >> $output_file
for i in {1..200}; do
    ./test_worker_queue $i >> $output_file
done

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file
# Esegui il secondo file 50 volte
echo "deque:  " >> $output_file
for i in {1..200}; do
    ./test_deque $i >> $output_file
done

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file
echo "Esecuzione completata. I risultati sono stati salvati in $output_file"

