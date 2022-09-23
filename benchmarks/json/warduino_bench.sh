#!/usr/bin/sh
# This scripts takes the name of a benchmark and runs it on an esp with arduino
################################################################################

tmpfile="$(mktemp --tmpdir)"
trap "rm '$tmpfile'" EXIT
cd "$(dirname "$0")"
outloc=${2:--}

PORT=/dev/ttyUSB0
FQBN=esp32:esp32:esp32wrover

echo $1 | tee -a $2

# load bench.wasm into ino
cp ./target/wasm32-unknown-unknown/release/$1.wasm ./bench.wasm
xxd -i bench.wasm > bench.h

# flash to device
arduino-cli compile --fqbn $FQBN json.ino
arduino-cli upload -p $PORT --fqbn $FQBN json.ino

# monitor device
arduino-cli monitor -p $PORT -c baudrate=115200

