all:
	$(MAKE) -C ../wa_sources
	g++ -g -x c ../mem.c ../util.c  -x c++ ../WARDuino.cpp ../primitives.cpp main.cpp -o warduino

clean:
	rm warduino
