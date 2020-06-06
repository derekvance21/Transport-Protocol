#!/bin/bash

declare -i i=1
for file in testfile monologue.txt act1.txt
do
    ./client 127.0.0.1 8080 $file
    diff $file $i.file
    i=$(( i + 1 ))
    echo "-----"
done
