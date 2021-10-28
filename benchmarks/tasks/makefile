TASKS=$(patsubst %/.,%,$(wildcard */.))

all: $(addsuffix /wast/arduino/arduino.ino,$(TASKS)) $(addsuffix /wast/impl.wast,$(TASKS)) $(addsuffix /wast/impl.wasm,$(TASKS)) $(addsuffix /c/arduino.ino,$(TASKS))

	echo $(TASKS)


%/wast/arduino/arduino.ino: %/wast/impl.wasm
	-mkdir $(@D)
	xxd -i $< $@
	sed -i 's/[^ ]*_impl_wasm/impl_wasm/'  $@
	cat ../arduino.ino.template >> $@

%/c/arduino.ino: %/wast/impl.c ../c.ino.template
	-mkdir $(@D)
	echo '#include "Arduino.h"' > $@
	echo '#pragma GCC optimize ("O0")' >> $@
	cat $^ | \
		sed '/#pragma \+nounroll/d' | \
		sed '/int *main() */s/int/int __attribute__((optimize("no-unroll-loops")))/' | \
		sed '/#include \+"Arduino.h"/d'>> $@

%/wast/impl.wasm:  %/wast/impl.c
	clang $< \
		--target=wasm32 \
		-O3 \
		-flto \
		-nostdlib \
		-Wl,--export-all \
		-Wl,--no-entry \
		-Wl,--lto-O3 \
		-o $@
	cat $@ > /dev/null

%/wast/impl.wast: %/wast/impl.wasm
	wasm2wat -f $< > $@

clean:
	-find -iname "impl.wast" -o -iname "impl.wasm" -o -iname "arduino.ino" | xargs rm

