#!/bin/bash
shopt -s nullglob
echo "Starting bach processing..."
for file in *.cnf *.CNF
do
    echo $file
    cnf2txt $file
done 
echo "End batch processing"
shopt -u nullglob
