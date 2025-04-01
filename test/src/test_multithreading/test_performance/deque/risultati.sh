#!/bin/bash

names=("test_deque_pop_back" "test_deque_pop_front" "test_deque_push_back" "test_deque_push_front")

make

for name in "${names[@]}"; do
  touch $name".txt"
  for i in {1..200}; do
      # Esegui il programma e salva l'output nel file
      ./$name>>$name".txt"
  done
done

