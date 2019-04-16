CXX       = g++
CC        = gcc 
CFLAGS    = -Wall -c
CXXFLAGS  = -v -std=c++11 -std=c++11 -stdlib=libc++ -Weverything

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

warduino: $(COBJECTS)  $(CXXSOURCES) 
	$(MKDIR)
	$(CXX) $(CXXFLAGS) $(COBJECTS) $(CXXSOURCES) -o $(OUTPUTDIR)$(TARGET)

.PHONY: clean
clean:
	$(RM) -rf $(OUTPUTDIR)
	$(RM) ./*.gc??
	$(RM) ./*.o
