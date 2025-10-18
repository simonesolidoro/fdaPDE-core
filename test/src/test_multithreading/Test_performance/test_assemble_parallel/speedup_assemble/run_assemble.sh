#!/bin/bash

#$2 nodi 

output_file="test_assemble_nodi"$1".txt"
touch "$output_file"


./"test_assemble" "$1" >> "$output_file" 

