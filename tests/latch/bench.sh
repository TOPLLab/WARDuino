#!/bin/bash

mkdir -p cleaned


for group in /home/tom/Arduino/libraries/WARDuino/tests/latch/scp/*/ ; do
    echo "running $group"
    
    time EMULATOR=/home/tom/Arduino/libraries/WARDuino/build-emu/wdcli WABT=/home/tom/Documents/TOPL/plugin/WABT/build/ CORESUITE=$group npm run spectest > /home/tom/Arduino/libraries/WARDuino/tests/latch/scp/$(basename $group).output
done

