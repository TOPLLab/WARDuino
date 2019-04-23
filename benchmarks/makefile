# __      __                  .___    .__               
#/  \    /  \_____ _______  __| _/_ __|__| ____   ____  
#\   \/\/   /\__  \\_  __ \/ __ |  |  \  |/    \ /  _ \ 
# \        /  / __ \|  | \/ /_/ |  |  /  |   |  (  <_> )
#  \__/\  /  (____  /__|  \____ |____/|__|___|  /\____/ 
#       \/        \/           \/             \/        
CXX       = g++
CC        = gcc
CFLAGS    = -Wall -c
CXXFLAGS  = -g -v -std=c++11  -Wall

OUTPUTDIR = ./bin/
MKDIR     = mkdir -p $(OUTPUTDIR)
TARGET    = warduino_benchmark

COBJECTS= \
	../mem.o\
	../util.o

CXXSOURCES = \
    	../WARDuino.cpp \
	../primitives.cpp \
    	benchmarks.cpp

all: warduino

../%.o : ../%.c
	$(CC) $(CFLAGS) -c $<   -o $@

../wa_sources/%.c: ../wa_sources/%.wast
	$(MAKE) -C ../wa_sources

warduino: $(COBJECTS)  $(CXXSOURCES) ../wa_sources/*.c
	$(MKDIR)
	$(CXX) $(CXXFLAGS) $(COBJECTS) $(CXXSOURCES) -o $(OUTPUTDIR)$(TARGET)


run: 
	./bin/$(TARGET)

.PHONY: clean
clean:
	$(MAKE) -C ../wa_sources clean
	$(RM) -rf $(OUTPUTDIR)
	$(RM) $(COBJECTS)
