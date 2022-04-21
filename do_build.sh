#!/bin/bash

cd build/
make && cd ..

echo Starting CLI

./build/wdcli --file examples/blinkm5stickc/blink.wasm
