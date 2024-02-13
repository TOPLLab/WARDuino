#!/bin/bash

mkdir -p cleaned

for file in /home/tom/Arduino/libraries/WARDuino/tests/latch/cleaned/*; do
    base=$(basename -- "$file")
    base="${base%%.*}"

    echo "copying $file and $base"

    cp /home/tom/Documents/TOPL/plugin/entire/${base}.wast /home/tom/Arduino/libraries/WARDuino/tests/latch/cleaned/
done
