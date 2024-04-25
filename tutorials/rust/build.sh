set -e

args=${1?Give an example as argument}
echo -e "Building example: ${args}"

example="examples/${args}"

cd ${example}

cargo build
cp target/wasm32-unknown-unknown/debug/examples.wasm ../../../../platforms/ESP-IDF/upload.wasm

cd ../../../../platforms/ESP-IDF/
xxd -i upload.wasm > upload.h

cd ../../

idf.py build

idf.py flash

