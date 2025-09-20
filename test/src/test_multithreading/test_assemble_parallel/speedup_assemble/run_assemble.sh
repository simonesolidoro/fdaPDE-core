#!/bin/bash

#$1 is the number of times the file is ran

#$2 nodi 

output_file="test_assemble_nodi"$2".txt"
touch "$output_file"


./"test_assemble" "$2" >> "$output_file" 

