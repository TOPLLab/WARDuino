#!/bin/bash

for file in /home/tom/Documents/TOPL/plugin/cleaned/*.asserts.wast; do
    base=$(basename -- "$file")
    base="${base%%.*}"

    echo "copying $file and $base"

    mkdir -p tmp
    cp $file tmp/
    cp $(dirname $file)/${base}.wast tmp/

    ls tmp/

    WABT=/home/tom/Documents/TOPL/plugin/WABT/build/ CORESUITE=$(realpath tmp)/ npm run spectest

    rm -rf tmp
done
