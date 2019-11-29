CC = g++
CXXFLAGS = -g -Wall -std=c++11

OBJS = main benchtest

HEADERS = coropp.h
MAINSRCS = main.cc coropp.cc
BENCHSRCS = benchtest.cc coropp.cc

all: $(OBJS)

main: $(MAINSRCS) $(HEADERS)
	$(CC) $(MAINSRCS) -o $@ $(CXXFLAGS)

benchtest: $(BENCHSRCS) $(HEADERS)
	$(CC) $(BENCHSRCS) -o $@ $(CXXFLAGS)

memcheck:
	sudo valgrind -q --leak-check=full --show-reachable=yes ./main

clean: 
	rm -rf $(OBJS)