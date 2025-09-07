#!/bin/bash
OUT=SP_assemble_parallel.txt

for i in {1..3}; do
    ./test_SP_assemble_parallel 500 1 1 >> "$OUT"
    echo >> "$OUT"
done

