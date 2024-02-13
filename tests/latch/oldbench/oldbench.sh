#!/bin/bash

mkdir -p cleaned


for group in /home/tom/Arduino/libraries/WARDuino/tests/latch/scp/*/ ; do
    echo "running $group"
    
    time ./run_spec_tests.py --compiler "/home/tom/Documents/TOPL/plugin/WABT/build/wat2wasm" --interpreter "./wdcli" --testsuite $group
done

