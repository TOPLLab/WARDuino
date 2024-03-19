#!/bin/bash

mkdir -p cleaned

for file in /home/tom/Documents/TOPL/plugin/entire/*.asserts.wast; do
    base=$(basename -- "$file")
    base="${base%%.*}"

    echo "copying $file and $base"

    mkdir -p tmp
    cp $file tmp/
    cp $(dirname $file)/${base}.wast tmp/

    ls tmp/

    WABT=/home/tom/Documents/TOPL/plugin/WABT/build/ CORESUITE=$(realpath tmp)/ npm run spectest > output

    while read line; do
    
      if grep -q "✔" <<< "$line"; then
        echo "$line" | grep -o "(.*$" | sed "s/(/&assert_return /" >> cleaned/${base}.asserts.wast
      elif grep -q "✖" <<< "$line"; then
        echo "removing $line" 
      fi
    done < output

    rm -rf tmp
    rm output
done
