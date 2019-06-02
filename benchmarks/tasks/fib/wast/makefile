all:
	emcc fib.c -s ONLY_MY_CODE=1 --ignore-dynamic-linking -s WASM=1 -O3 -s NO_FILESYSTEM=1  -o fib.wasm -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']"
	wasm2wat -f fib.wasm > fib.wast
	cp fib.wasm target_data
	test -d arduino || mkdir arduino
	(xxd -i target_data - ; cat ../../../arduino.ino.template) > arduino/arduino.ino
	rm target_data

clean:
	rm *.html *.js *.wasm *.wast  arduino.ino || true