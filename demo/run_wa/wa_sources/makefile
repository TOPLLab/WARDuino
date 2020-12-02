all: hello_world.c get.c setup.c loop.c

%.wasm: %.wast
	wat2wasm -o $@ $<

%.wasm.dbg: %.wast
	wat2wasm -v $< >> $@

%.c: %.wasm %.wasm.dbg
	xxd -i $< > $@
	echo "/*" >> $@
	cat $<.dbg >> $@
	echo "*/" >> $@
	cat $@

clean:
	rm *.o *.c *.dbg *.wasm || true
