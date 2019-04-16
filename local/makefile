CXX       = g++
CC        = gcc
CFLAGS    = -Wall -c
CXXFLAGS  = -g -v -std=c++11  -Wall

OUTPUTDIR = ./bin/
MKDIR     = mkdir -p $(OUTPUTDIR)
TARGET    = warduino

COBJECTS= \
	../mem.o\
	../util.o

CXXSOURCES = \
    	../WARDuino.cpp \
	../primitives.cpp \
    	main.cpp

all: warduino

../%.o : ../%.c
	$(CC) $(CFLAGS) -c $<   -o $@

../wa_sources/%.c: ../wa_sources/%.wast
	$(MAKE) -C ../wa_sources

warduino: $(COBJECTS)  $(CXXSOURCES) ../wa_sources/*.c
	$(MKDIR)
	$(CXX) $(CXXFLAGS) $(COBJECTS) $(CXXSOURCES) -o $(OUTPUTDIR)$(TARGET)

.PHONY: clean
clean:
	$(MAKE) -C ../wa_sources clean
	$(RM) -rf $(OUTPUTDIR)
	$(RM) $(COBJECTS)
