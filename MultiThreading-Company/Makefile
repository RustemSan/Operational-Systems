CXX=g++
LD=g++
AR=ar
CXXFLAGS=-std=c++20 -Wall -pedantic -O2
SHELL:=/bin/bash
MACHINE=$(shell uname -m)-$(shell echo $$OSTYPE)

# Notes for arm64-darwin24:
# the library exists in two variants:
# - clang version
# - g++ version
# change the symlink to the correct version based on the compiler you use:
#  the default is clang,
#  there may exist a symlink g++ -> clang++
# use g++ --version to determine the exact compiler version

all: test

deps:
	$(CXX) -MM *.cpp > Makefile.d

test: solution.o sample_tester.o
	$(LD) $(CXXFLAGS) -o $@ $^ -L./$(MACHINE) -lprogtest_solver -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

lib: progtest_solver.o
	mkdir -p $(MACHINE)
	$(AR) cfr $(MACHINE)/libprogtest_solver.a $^

clean:
	rm -f *.o test *~ core sample.tgz Makefile.d

pack: clean
	rm -f sample.tgz
	tar zcf sample.tgz --exclude progtest_solver.cpp *


-include Makefile.d
