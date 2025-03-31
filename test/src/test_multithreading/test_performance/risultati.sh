#!/bin/bash

# Nome del file di output dove salvare tutti i risultati
output_file="prova.txt"

# Aggiungi una riga vuota per separare i risultati
echo "" >> $output_file

#titolo
echo "pop_back() di 16000 elementi messo option in deque" >> $output_file

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

