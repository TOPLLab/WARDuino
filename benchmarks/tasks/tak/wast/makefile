all:
	emcc tak.c -s ONLY_MY_CODE=1 --ignore-dynamic-linking -s WASM=1 -O3 -s NO_FILESYSTEM=1  -o tak.wasm -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall', 'cwrap']"
	wasm2wat -f tak.wasm > tak.wast
	cp tak.wasm target_data
	test -d arduino || mkdir arduino
	(xxd -i target_data - ; cat ../../../arduino.ino.template) > arduino/arduino.ino
	rm target_data

clean:
	rm *.html *.js *.wasm *.wast  arduino.ino || true