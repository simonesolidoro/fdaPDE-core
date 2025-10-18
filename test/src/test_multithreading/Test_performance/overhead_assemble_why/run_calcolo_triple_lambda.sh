#!/bin/bash

#$1 nodi 

output_file="test_calcolo_triple_lambda_nodi"$1".txt"
touch "$output_file"


./"test_calcolo_triple_lambda" "$1" >> "$output_file" 
    
