TASKS=$(patsubst %/.,%,$(wildcard */.))

all: $(addsuffix /wast/warduino/warduino.ino,$(TASKS)) $(addsuffix /wast/edward/edward.ino,$(TASKS)) $(addsuffix /wast/wasm3/wasm3.ino,$(TASKS)) $(addsuffix /wast/impl.wast,$(TASKS)) $(addsuffix /wast/impl.wasm,$(TASKS)) $(addsuffix /c/c.ino,$(TASKS))

	echo $(TASKS)


%/wast/warduino/warduino.ino: %/wast/impl.wasm
	-mkdir $(@D)
	xxd -i $< $@
	sed -i 's/[^ ]*_impl_wasm/impl_wasm/' $@
	cat ../warduino.ino.template >> $@

%/wast/edward/edward.ino: %/wast/impl.wasm
	-mkdir $(@D)
	xxd -i $< $@
	sed -i 's/[^ ]*_impl_wasm/impl_wasm/' $@
	cat ../edward.ino.template >> $@

%/wast/wasm3/wasm3.ino: %/wast/impl.wasm
	-mkdir $(@D)
	xxd -i $< $@
	sed -i 's/[^ ]*_impl_wasm/impl_wasm/'  $@
	cat ../wasm3.ino.template >> $@

%/c/c.ino: %/wast/impl.c ../c.ino.template
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
		-Oz \
		-flto \
		-nostdlib \
		-Wl,--export-all \
		-Wl,--no-entry \
		-Wl,--strip-all \
		-Wl,--lto-O3 \
		-o $@
	cat $@ > /dev/null

%/wast/impl.wast: %/wast/impl.wasm
	wasm2wat -f $< > $@

clean:
	-find -iname "impl.wast" -o -iname "impl.wasm" -o -iname "c.ino" -o -iname "warduino.ino" -o -iname "edward.ino" -o -iname "wasm3.ino" | xargs rm

