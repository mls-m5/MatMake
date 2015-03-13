
TESTSRC = $(wildcard src/*.cpp)
TESTOBJ = $(TESTSRC:.cpp=.o)
CXXFLAGS = -std=c++11

all: matdep .depend $(TESTOBJ) 

matdep: matdep.cpp

.depend: matdep Makefile $(TESTSRC)
	./matdep $(TESTSRC) > .depend
	./matdep $(TESTSRC) ##For visible output

include .depend
