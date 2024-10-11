all: add.wasm matmul.wasm
.PHONY: all

%.wasm: %.c Makefile
	clang --target=wasm32 -msimd128 -O3 -flto -nostdlib -Wl,--no-entry -Wl,--export-all -o $@ $<

clean:
	rm -f *.wasm
.PHONY: clean