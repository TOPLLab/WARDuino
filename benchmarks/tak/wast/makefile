all:
	emcc tak.c -s ONLY_MY_CODE=1 --ignore-dynamic-linking -s WASM=1 -O3 -s NO_FILESYSTEM=1  -o tak.wasm -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']"
	wasm2wat tak.wasm > tak.wast

clean:
	rm *.html *.js *.wasm *.wast || true