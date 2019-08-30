DEBUGFLAGS = -DDEBUG=0 -DTRACE=1 -DINFO=1 -DWARN=1
CXX       = g++
CC        = gcc
CFLAGS    = -g -Wall -c
CXXFLAGS  = -g -v -std=c++11  -Wall

CXXSOURCES = \
	../../mem.cpp \
	../../util.cpp \
	../../util_arduino.cpp \
	../../interrupt_operations.cpp \
	../../debug.cpp \
	../../WARDuino.cpp \
	../../primitives.cpp \
	../../instructions.cpp \


all: display.ino

%.run: %.local.cpp
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) $(COBJECTS) $(CXXSOURCES) $< -o $@

%.ino: %.wasm.c ../main.template.ino
	cat $< > $@
	sed 's/{{src}}/$(subst .,_,$(basename $<))/g' ../main.template.ino >> $@

%.local.cpp:  %.wasm.c ../main.template.cpp
	cat $< > $@
	sed 's/{{src}}/$(subst .,_,$(basename $<))/g' ../main.template.cpp >> $@

%.wasm:  %.c
	emcc $< -s ONLY_MY_CODE=1 --ignore-dynamic-linking -s WASM=1 -O3 -s NO_FILESYSTEM=1 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -o $@


%.wasm.c : %.wasm
	xxd -i $< > $@


%.wast: %.wasm
	wasm2wat -f $< > $@

up:	display.ino
	../../scripts/upload ESP32 $<

clean:
	rm *.html *.js *.wasm *.wast *.wasm.c *.ino *.local.cpp *.run || true
