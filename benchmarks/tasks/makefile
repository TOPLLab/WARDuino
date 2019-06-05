TASKS=$(patsubst %/.,%,$(wildcard */.))

all: $(addsuffix /wast/arduino/arduino.ino,$(TASKS)) $(addsuffix /wast/impl.wast,$(TASKS)) $(addsuffix /wast/impl.wasm,$(TASKS))
	echo $(TASKS)


%/wast/arduino/arduino.ino: %/wast/impl.wasm
	-mkdir $(@D)
	(xxd -i $< - | sed 's/[^ ]*_impl_wasm/impl_wasm/'; cat ../arduino.ino.template) > $@

%/wast/impl.wasm:  %/wast/impl.c
	emcc $< -s ONLY_MY_CODE=1 --ignore-dynamic-linking -s WASM=1 -O3 -s NO_FILESYSTEM=1  -o $@

%/wast/impl.wast: %/wast/impl.wasm
	wasm2wat -f $< > $@

clean:
	-find -iname "impl.wast" -o -iname "impl.wasm" -o -iname "arduino.ino" | xargs rm


lolz:
	echo $(patsubst %/.,%,$(wildcard */.))
